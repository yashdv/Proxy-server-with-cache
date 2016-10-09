# Proxy-server-with-cache

To use the Proxy server follow the commands:

1. Run the command "g++ thread.cpp" to compile the proxy server.
2. Execute ./a.out to start the proxy server.
3. In a new terminal (same directory) run the command "gcc mytcpclient -o client" to compile the client.
4. Execute ./client to start the command.
5. Follow the instructions on the terminals.

Note: 

The server makes the request sent by client to "proxy.iiit.ac.in"
If you want to make to the host directly then replace the line 177 in thread.cpp by 
	"if ((rv = getaddrinfo(hostname, "80", &hints, &servinfo)) != 0)"
