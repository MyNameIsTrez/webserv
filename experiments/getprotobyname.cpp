#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>

// c++ getprotobyname.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
int main(void)
{
	char protocol[][42] = {
		"xd",
		"icmp",
		"ggp",
		"tcp",
		"egp",
		"pup",
		"udp",
		"hmp",
		"xns-idp",
		"rdp",
		"rvd",
		"ip",
		"Ip",
		"iP",
		"IP",
	};

	size_t length = sizeof(protocol) / sizeof(*protocol);
	for (size_t i = 0; i < length; i++)
	{
		if (i > 0)
		{
			printf("\n");
		}

		protoent *servptr = getprotobyname(protocol[i]);

		if (servptr == NULL)
		{
			printf("getprotobyname(\"%s\") is ERR\n", protocol[i]);
		}
		else
		{
			// Print the related info
			printf("getprotobyname(\"%s\") is OK\n", protocol[i]);
			printf(" Official name is %s\n", servptr->p_name);
			printf(" Protocol number is %d\n", servptr->p_proto);
			while (*servptr->p_aliases)
			{
				printf("  Alias is %s\n", *servptr->p_aliases);
				servptr->p_aliases++;
			}
		}
	}

	for (int i = 0; i < 100000; i++)
	{
		protoent *servptr = getprotobyname("tcp");
		if (servptr == NULL) EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
