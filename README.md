# BindToInterface - securely bind a programm to a specific network interface

Bind To Interface is a useful program if you have multiple network interfaces and want your program use strictly only one or none. You don't
have to bother with routing tables, network namespaces or iptables and marking packets.

BindToInterface is very flexible. You can set exceptions to which IPs no binding should be made. This is especially useful if your program still needs 
to communicate with localhost. For example if you make an outgoing ssh connection, bind it to eth1 it couldn't reach localhost without the bind exception.

## How does bind to network interface work?

BindToInterface uses a feature called dynamic linking. Every program that makes an internet connection does this by making an API call to the operating system. In this case this API function is called `connect`.
To bind to a specific network interface this program intercepts this call, decides if it has to bind to an interface and then calls the original connect function of the operating system. 
Because of this method BindToInterface should not leak any data through an undesired interface. If you make other experiences, please open an issue. 

## Build BindToInterface

Download the source code and compile it with `gcc -nostartfiles -fpic -shared bindToInterface.c -o bindToInterface.so -ldl -D_GNU_SOURCE`

## Usage of BindToInterface

Configuration and usage of bind to interface is very simple. Example of how to run a programm with BindToInterface: `BIND_INTERFACE=ovpn DNS_OVERRIDE_IP=8.8.8.8 LD_PRELOAD=./bindToInterface.so curl ifconfig.me`
Instead of specifying the parameters in the same line you could also export them like `export DNS_OVERRIDE_IP=8.8.8.8`. In this case interface ovpn is a VPN provider but configured via option `route-nopull`, 
so your internet traffic still goes via your normal internet connection without VPN.

### BIND_INTERFACE

This is the essential part. If not specified program will warn you, however it will not refuse the connection. 

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
