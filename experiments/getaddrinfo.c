#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
// #include <unistd.h>

// gcc getaddrinfo.c -Wall -Wextra -Werror -Wpedantic -Wfatal-errors -g && ./a.out google.com
int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Usage: %s <hostname>\n", argv[0]);
		return EXIT_FAILURE;
	}

	const char *hostname = argv[1];

	struct addrinfo hint;
	struct addrinfo *result;

	memset(&hint, 0, sizeof(hint));

	// AF stands for Address Family
	// hint.ai_family = AF_INET; // IPV4 addresses
	// hint.ai_family = AF_INET6; // IPV6 addresses
	hint.ai_family = AF_UNSPEC; // Unspecified: give all addresses

	// Leaving this commented out means it'll give both streams (TCP) and datagrams (UDP)
	// hint.ai_socktype = SOCK_STREAM;

	// You can use this to have getaddrinfo() fill in the required port numbers
	// in result, as well as the address, family, and protocol info
	// It is either a numeric port number expressed as a string("80"),
	// or a defined service name("http")
	// It will fill in the corresponding port number in the returned sockaddr structs,
	// so you can pass them straight to connect() or bind()
	const char *service_name = NULL;

	if (getaddrinfo(hostname, service_name, &hint, &result))
	{
		perror("getaddrinfo");
		return EXIT_FAILURE;
	}

	struct addrinfo *tmp = result;
	while (tmp)
	{
		printf("Entry:\n");
		printf("\tType: %i\n", tmp->ai_socktype);
		printf("\tFamily: %i\n", tmp->ai_family);

		// IPv4 will also fit with INET6_ADDRSTRLEN, since it is 46:
		// https://stackoverflow.com/a/7477384/13279557
		char address_string[INET6_ADDRSTRLEN];
		void *address;

		if (tmp->ai_family == AF_INET)
		{
			// IPv4
			address = &((struct sockaddr_in *)tmp->ai_addr)->sin_addr;
		}
		else
		{
			// IPv6
			address = &((struct sockaddr_in6 *)tmp->ai_addr)->sin6_addr;
		}

		// Network representation to printable string
		inet_ntop(tmp->ai_family, address, address_string, INET6_ADDRSTRLEN);

		printf("\tAddress: %s\n", address_string);

		tmp = tmp->ai_next;
	}

	freeaddrinfo(result);

    return EXIT_SUCCESS;
}
