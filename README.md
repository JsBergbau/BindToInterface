# BindToInterface - securely bind a programm to a specific network interface or network adapter

Bind To Interface is a useful program for unix/linux if you have multiple network interfaces or adapters and want your program use strictly only one or none. You don't
have to bother with routing tables, network namespaces or iptables and marking packets.

BindToInterface is very flexible. You can set exceptions to which IPs no binding should be made. This is especially useful if your program still needs 
to communicate with localhost. For example if you make an outgoing ssh connection, bind it to eth1 it couldn't reach localhost without the bind exception.

## How does bind to network interface work?

BindToInterface uses a feature called dynamic linking. Every program that makes an internet connection does this by making an API call to the operating system. In this case this API function is called `connect`.
To bind to a specific network interface this program intercepts this call, decides if it has to bind to an interface and then calls the original connect function of the operating system. 
**This works only if standard libraries are used. When using go programs for example, which uses static linked libraries, then this won't work and data would get leaked.**
If you know other examples where binding doesn't work, please open an issue. 

## Build BindToInterface

Download the source code and compile it with `gcc -nostartfiles -fpic -shared bindToInterface.c -o bindToInterface.so -ldl -D_GNU_SOURCE`

## Usage of BindToInterface

Configuration and usage of bind to interface is very simple. Example of how to run a programm with BindToInterface: `BIND_INTERFACE=ovpn DNS_OVERRIDE_IP=8.8.8.8 LD_PRELOAD=./bindToInterface.so curl ifconfig.me` Note: Prior to kernel 5.6.X - April 2020 you need superuser rights, see https://github.com/JsBergbau/BindToInterface/issues/4
Instead of specifying the parameters in the same line you could also export them like `export DNS_OVERRIDE_IP=8.8.8.8`. In this case interface ovpn is a VPN provider but configured via option `route-nopull`, 
so your internet traffic still goes via your normal internet connection without VPN.

### Multiple physical NICs
When using BindToInterface with OpenVPN and `route-nopull` option everything works as expected, even though `ip route` does only show the local subnet rules like `10.8.0.0/16 dev ovpn proto kernel scope link src 10.8.0.12`.

However when using pyhsical interfaces, kernel needs to know your gateway for the pyhsical interfaces. So when using like `BIND_INTERFACE=eth1 DNS_OVERRIDE_IP=8.8.8.8 LD_PRELOAD=./bindToInterface.so curl 34.160.111.145 -H "Host: ifconfig.me"` and you get an error "No route to host" then you need to add a route for your second NIC. You can to this with a higher metric, so this route is not used by default, but only with BindToInterface like `sudo ip route add default via 192.168.194.1 metric 500`. A route without a metric has metric `0` meaning highest metric, see https://unix.stackexchange.com/a/431839
 

### Specifying absolute path 

Especially when using more complex scripts or programs, that use another working directory than the current one, you have to specify the absolute path to `bindToInterface.so`, otherwise an error message will be printed, that it couldn't be loaded. Example when bindToInterface.so is located at `/opt/bindToInterface/bindToInterface.so`: `BIND_INTERFACE=ovpn DNS_OVERRIDE_IP=8.8.8.8 LD_PRELOAD=/opt/bindToInterface/bindToInterface.so curl ifconfig.me`

### BIND_INTERFACE

This is the essential part. If not specified program will warn you, however it will not refuse the connection. 

### BIND_SOURCE_IP (unreliable / only on some systems)

On some systems you can also set the source IP in case multiple IP addresses are attached to your interface, like this: `BIND_INTERFACE=eth0 BIND_SOURCE_IP=1.2.3.4 LD_PRELOAD...`

**Warning**: It highly depends on which system you are using BIND_SOURCE_IP. Kernel versions 5.10 seem not to work, whereas Kernel 5.15 seems to work, see https://github.com/JsBergbau/BindToInterface/pull/12#issuecomment-1776647513

**Use at your own risk and try if it works on your system**!

### DNS_OVERRIDE_IP

When you have multiple interfaces you normally also have multiple DNS servers. Since your program is bound to specified interface, also DNS traffic has to go through that interface. 
If you don't specify this parameter, your program will try the DNS servers sequentially which causes unnecessary delay.

### DNS_OVERRIDE_PORT

If you have DNS servers not listening on port 53 you can use this option. 

### BIND_EXCLUDE

This is also a very useful option. You can specify here destination IP adresses which are excluded from interface binding. In the example of the first paragraph you would execute your ssh like
`BIND_INTERFACE=ovpn DNS_OVERRIDE_IP=8.8.8.8 BIND_EXCLUDE=127.0.0.1 LD_PRELOAD=./bindToInterface.so ssh <your ssh options>` So SSH can connect to your localhost.
You can specifiy multiple IPs by seperating them by comma, so `BIND_EXCLUDE=127.0.0.1,192.168.0.1` and so on. Multiple IPs are also possible. So if you want to exclude binding to your whole 192.168.0.0/24 
subnet, you can use `BIND_EXCLUDE=127.0.0.1,192.168.0`. The program checks if destination IP starts with one IP from bind exclude. If so this connection isn't bound to interface. If you want to exclude 192.168.0.0/16
subnet, you would use `BIND_EXCLUDE=127.0.0.1,192.168.`. Since it is checked if destination IP begins with one in BIND_EXCLUDE specified address, it doesn't matter if you specify it with or without trailing `.`.

## Example usage of BindToInterface

### Using interactively

Instead of executing each program with LD_PRELOAD you can use `BIND_INTERFACE=ovpn DNS_OVERRIDE_IP=8.8.8.8 BIND_EXCLUDE=8.8.8 LD_PRELOAD=./bindToInterface.so bash`
In this second bash all programs executed now use bound interface, try it with `curl ifconfig.me`. This will give you the IP "ovpn" interface.

### Microsocks

You can for example use this program to bind microsocks https://github.com/rofl0r/microsocks to one desired interface. You can have multiple VPN connections on Raspberry PI for example and then you only need
to specify a SOCKS proxy in your browser to have different IPs from multiple countries. Please don't use this for anonymization, because via Websockets and so on, there may be a potential leak of your
true IP address. Since microsocks opens "connect sockets" only for outgoing traffic you don't have to use bind exclude for your clients. Clients are handled via "accept sockets" and there is no binding.
Example command line: `BIND_INTERFACE=ovpn DNS_OVERRIDE_IP=8.8.8.8 LD_PRELOAD=./bindToInterface.so ./microsocks -p 1080`

### CAUTION when using sudo

It is ok to use `sudo BIND_INTERFACE=ovpn DNS_OVERRIDE_IP=8.8.8.8 BIND_EXCLUDE=8.8.8 LD_PRELOAD=./bindToInterface.so bash` or any other program. BUT NEVER use sudo again in front of the program. This will lead that bindToInterface will not be loaded and thus the wrong interface will be used.

### CAUTION when using root rights in general especially with network tools like nmap 
**Network tools like nmap will use RAW sockets when using root rights. This means they will not use the OS systemcall. So interface will not be bound and nmap will use your default route like executing without LD_PRELOAD. In the case of nmap there is the `-e` option where you can instruct nmap to use specified interface. So use this option for nmap and never LD_PRELOAD with nmap.**


## Debug and tests

If you have for example `BIND_INTERFACE=ovpn DNS_OVERRIDE_IP=8.8.8.8 BIND_EXCLUDE=8.8.8 LD_PRELOAD=./bindToInterface.so traceroute -U -p 53 8.8.8.8` then you can see that DNS traffic will
get routed via your default route.
If you use `BIND_INTERFACE=ovpn DNS_OVERRIDE_IP=8.8.8.8 LD_PRELOAD=./bindToInterface.so traceroute -U -p 53 8.8.8.8` then you see that DNS traffic is routed via your bound interface.

You can also uncomment line `//#define DEBUG` and compile to get some Debug outputs of what this program is doing. Use this especially via commands like 
`BIND_INTERFACE=ovpn LD_PRELOAD=./bindToInterface.so curl ifconfig.me` 
Then you will see that curl uses multiple DNS servers from `/etc/resolv.conf` until it finds one that is reachable via your bound interface and thus takes some time until it gets your IP.

If you use `BIND_INTERFACE=ovpn DNS_OVERRIDE_IP=8.8.8.8 LD_PRELOAD=./bindToInterface.so curl ifconfig.me` then DNS server address is rewritten to go through VPN and thats why curl will report your IP very fast.

### IPv6

Currently there is no support of IPv6, because I just don't have an IPv6 connection to test. If you need it, open an issue and I'll make a version that you can test.
