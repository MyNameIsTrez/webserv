#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

// clear && c++ getaddrinfo.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out google.com
int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " <hostname>" << std::endl;
		return EXIT_FAILURE;
	}

	const char *hostname = argv[1];

	// You can use this to have getaddrinfo() fill in the required port numbers
	// in result, as well as the address, family, and protocol info
	// It is either a numeric port number expressed as a string("80"),
	// or a defined service name("http")
	// It will fill in the corresponding port number in the returned sockaddr structs,
	// so you can pass them straight to connect() or bind()
	const char *service = NULL;
	// const char *service = "80";
	// const char *service = "81";
	// const char *service = "8080";

	addrinfo hint;
	bzero(&hint, sizeof(hint));

	// AF stands for Address Family
	// hint.ai_family = AF_INET; // IPv4 addresses
	// hint.ai_family = AF_INET6; // IPv6 addresses
	// TODO: Why doesn't this print both IPv4 and IPv6?
	hint.ai_family = AF_UNSPEC; // Unspecified: give all addresses

	// Leaving this commented out means it'll give both streams (TCP) and datagrams (UDP)
	// hint.ai_socktype = SOCK_STREAM;

	addrinfo *result;

	// TODO: Replace &hint with NULL?
	int status = getaddrinfo(hostname, service, &hint, &result);
	if (status != 0)
	{
		std::cerr << "getaddrinfo: " << status << " " << gai_strerror(status) << std::endl;
		return EXIT_FAILURE;
	}

	addrinfo *tmp = result;
	while (tmp)
	{
		std::cout << "Entry:" << std::endl;
		std::cout << "\tFamily: " << tmp->ai_family << std::endl;
		std::cout << "\tType: " << tmp->ai_socktype << std::endl;
		std::cout << "\tProtocol: " << tmp->ai_protocol << std::endl;

		// IPv4 will also fit within INET6_ADDRSTRLEN:
		// https://stackoverflow.com/a/7477384/13279557
		char address_string[INET6_ADDRSTRLEN];
		void *address;

		in_port_t port;

		if (tmp->ai_family == AF_INET)
		{
			// IPv4
			address = &((sockaddr_in *)tmp->ai_addr)->sin_addr;
			port = ((sockaddr_in *)tmp->ai_addr)->sin_port;
		}
		else
		{
			// IPv6
			address = &((sockaddr_in6 *)tmp->ai_addr)->sin6_addr;
			port = ((sockaddr_in6 *)tmp->ai_addr)->sin6_port;
		}

		// Network representation to printable string
		inet_ntop(tmp->ai_family, address, address_string, INET6_ADDRSTRLEN);
		std::cout << "\tAddress: " << address_string << std::endl;

		// inet_ntop(tmp->ai_family, address, address_string, INET6_ADDRSTRLEN);
		std::cout << "\tPort: " << port << std::endl;

		tmp = tmp->ai_next;
	}

	freeaddrinfo(result);

	return EXIT_SUCCESS;
}
