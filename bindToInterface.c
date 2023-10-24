#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dlfcn.h>
#include <net/if.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

//#define DEBUG

//compile with gcc -nostartfiles -fpic -shared bindToInterface.c -o bindToInterface.so -ldl -D_GNU_SOURCE
//Use with BIND_INTERFACE=<network interface> LD_PRELOAD=./bindInterface.so <your program> like curl ifconfig.me

int bind_to_source_ip(int sockfd, const char *source_ip)
{
    struct sockaddr_in source_addr;
    memset(&source_addr, 0, sizeof(source_addr));
    source_addr.sin_family = AF_INET;
    source_addr.sin_addr.s_addr = inet_addr(source_ip);

    return bind(sockfd, (struct sockaddr *)&source_addr, sizeof(source_addr));
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	int *(*original_connect)(int, const struct sockaddr *, socklen_t);
	original_connect = dlsym(RTLD_NEXT, "connect");

	static struct sockaddr_in *socketAddress;
	socketAddress = (struct sockaddr_in *)addr;

	char *dest = inet_ntoa(socketAddress->sin_addr);

	if (socketAddress->sin_family == AF_INET)
	{
		unsigned short port = ntohs(socketAddress->sin_port);

		char *DNSIP_env = getenv("DNS_OVERRIDE_IP");
		char *DNSPort_env = getenv("DNS_OVERRIDE_PORT");
		int port_new = port;

		if (port == 53 && DNSIP_env != NULL && strlen(DNSIP_env) > 0)
		{
			if (DNSPort_env != NULL && strlen(DNSPort_env) > 0)
			{
				port_new = atoi(DNSPort_env);
				socketAddress->sin_port = htons(port_new);
			}
#ifdef DEBUG
			printf("Detected DNS query to: %s:%i, overwriting with %s:%i \n", dest, port, DNSIP_env, port_new);
#endif
			socketAddress->sin_addr.s_addr = inet_addr(DNSIP_env);
		}
		port = port_new;
		dest = inet_ntoa(socketAddress->sin_addr); //with #include <arpa/inet.h>

#ifdef DEBUG
		printf("connecting to: %s:%i \n", dest, port);
#endif

		bool IPExcluded = false;
		char *bind_excludes = getenv("BIND_EXCLUDE"); 
		if (bind_excludes != NULL && strlen(bind_excludes) > 0)
		{
			bind_excludes = (char*) malloc(strlen(getenv("BIND_EXCLUDE")) * sizeof(char) + 1);
			strcpy(bind_excludes,getenv("BIND_EXCLUDE"));
			char sep[] = ",";
			char *iplist;
			iplist = strtok(bind_excludes, sep);
			while (iplist != NULL)
			{
				if(!strncmp(dest,iplist,strlen(iplist)))
				{
					IPExcluded = true;
#ifdef DEBUG
					printf("IP %s excluded by IP-List, not binding to interface %s\n", dest, getenv("BIND_INTERFACE"));
#endif
					break;
				}
				iplist = strtok(NULL, sep);
			}
			free(bind_excludes);
		}

		if (!IPExcluded) //Don't bind when destination is localhost, because it couldn't be reached anymore
		{
			char *bind_addr_env;
			bind_addr_env = getenv("BIND_INTERFACE");
		    	char *source_ip_env;
			source_ip_env = getenv("BIND_SOURCE_IP");
			struct ifreq interface;

			int errorCode;
			if (bind_addr_env != NULL && strlen(bind_addr_env) > 0)
			{
				// printf(bind_addr_env);

				struct ifreq boundInterface =
					{
						.ifr_name = "none",
					};
				socklen_t optionlen = sizeof(boundInterface);
				errorCode = getsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, &boundInterface, &optionlen);
				if (errorCode < 0)
				{
					//getsockopt should not fail
					perror("getsockopt");
					return -1;
				};
#ifdef DEBUG
				printf("Bound Interface: %s.", boundInterface.ifr_name);
#endif

				if (!strcmp(boundInterface.ifr_name, "none") || strcmp(boundInterface.ifr_name, bind_addr_env))
				{
#ifdef DEBUG
					printf(" Socket not bound to desired interface (Bound to: %s). Binding to interface: %s\n", boundInterface.ifr_name, bind_addr_env);
#endif
					strcpy(interface.ifr_name, bind_addr_env);
					errorCode = setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, &interface, sizeof(interface)); //Will fail if socket is already bound to another interface
					if (errorCode < 0)
					{
						perror("setsockopt");
						errno = ENETUNREACH; //Let network appear unreachable for maximum security when desired interface is not available
						return -1;
					};
				}
			}
			
			if(source_ip_env != NULL && strlen(source_ip_env) > 0){
				if (bind_to_source_ip(sockfd, source_ip_env) < 0){
				    perror("bind_to_source_ip failed");
				    return -1;
				}
			}
			
			if(!(source_ip_env != NULL && strlen(source_ip_env) > 0) && !(bind_addr_env != NULL && strlen(bind_addr_env) > 0)){
				printf("Warning: Programm with LD_PRELOAD started, but BIND_INTERFACE environment variable not set\n");
				fprintf(stderr, "Warning: Programm with LD_PRELOAD startet, but BIND_INTERFACE environment variable not set\n");
			}
		}
	}

	return (uintptr_t)original_connect(sockfd, addr, addrlen);
	
}
