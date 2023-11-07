#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

// clear && c++ getaddrinfo_identical.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
int main()
{
	addrinfo hint1{};
	hint1.ai_family = AF_INET;
	hint1.ai_socktype = SOCK_STREAM;
	hint1.ai_protocol = getprotobyname("tcp")->p_proto;
	addrinfo *result1;
	// int status1 = getaddrinfo("google.com", "8080", &hint1, &result1);
	int status1 = getaddrinfo("localhost", "80", &hint1, &result1);

	addrinfo hint2{};
	addrinfo *result2;
	int status2 = getaddrinfo("127.0.0.1", "81", &hint2, &result2);

	std::cout << status1 << std::endl;
	std::cout << result1 << std::endl;
	std::cout << status2 << std::endl;
	std::cout << result2 << std::endl;
	assert(status1 == status2);

	// std::cout << "getaddrinfo 1 address: " << &((sockaddr_in *)tmp->ai_addr)->sin_addr << std::endl;

	std::cout << "getaddrinfo 1" << std::endl;
	addrinfo *tmp = result1;
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

	std::cout << std::endl;
	std::cout << "getaddrinfo 2" << std::endl;
	tmp = result2;
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

	freeaddrinfo(result1);
	freeaddrinfo(result2);

	return EXIT_SUCCESS;
}
