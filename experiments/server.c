#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 18000

#define MAX_CONNECTION_QUEUE_LENGTH 10

#define MAX_RECEIVED_LEN 4096
#define MAX_SENT_LEN 4096

#define TCP 0

void die(const char *fmt, ...)
{
	int errno_copy = errno;

	// Print out the fmt + args to stderr
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n");
	fflush(stderr);

	// Print out error message if errno was set
	if (errno_copy != 0)
	{
		fprintf(stderr, "errno %d: %s\n", errno_copy, strerror(errno_copy));
		fflush(stderr);
	}

	exit(EXIT_FAILURE);
}

char *bin2hex(char *input, size_t len)
{
	// A hexit is a hexadecimal digit
	char *hexits = "0123456789ABCDEF";

	if (input == NULL || len <= 0)
	{
		return NULL;
	}

	// (2 hexits + space) per char from input: "AA BB "
	int result_length = len * 3;

	char *result = malloc(result_length + 1);
	bzero(result, result_length + 1);

	for (size_t i = 0; i < len; i++)
	{
		// This shifting and masking assumes that input is unsigned
		result[i * 3 + 0] = hexits[input[i] >> 4];
		result[i * 3 + 1] = hexits[input[i] & 0x0F];
		result[i * 3 + 2] = ' ';
	}

	return result;
}

// gcc server.c -Wall -Wextra -Werror -Wpedantic -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
// Code stolen from https://youtu.be/esXw4bdaZkc
int main(void)
{
	int socket_fd;
	if ((socket_fd = socket(AF_INET, SOCK_STREAM, TCP)) < 0)
	{
		die("socket");
	}

	// "the parameter should be non-zero to enable a boolean option"
	int option = 1;
	// Enables local address reuse
	// This prevents socket() calls from possibly returning an error on server restart
	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERVER_PORT);

	if ((bind(socket_fd, (struct sockaddr *)&servaddr, sizeof(servaddr))) < 0)
	{
		die("bind");
	}

	if ((listen(socket_fd, MAX_CONNECTION_QUEUE_LENGTH)) < 0)
	{
		die("listen");
	}

	char received[MAX_RECEIVED_LEN + 1];
	char *sent = "HTTP/1.0 200 OK\r\n\r\n<H1>Hello";

	while (true)
	{
		printf("Waiting for a connection on port %d\n", SERVER_PORT);
		fflush(stdout);

		// TODO: Simplify this man-page explanation:
		// The argument address is a result parameter that is filled in with the address of the
		// connecting entity, as known to the communications layer.
		// The exact format of the address parameter is determined by the
		// domain in which the communication is occurring.
		// The address_len is a value-result parameter;
		// it should initially contain the amount of space pointed to by address;
		// on return it will contain the actual length (in bytes) of the address returned.
		// This call is used with connection-based socket types, currently with SOCK_STREAM.
		struct sockaddr address;
		socklen_t address_len = sizeof(address);
		int client_fd = accept(socket_fd, &address, &address_len);
		printf("Address length: %d\n", address_len);

		// // IPv4 will also fit with INET6_ADDRSTRLEN, since it is 46:
		// // https://stackoverflow.com/a/7477384/13279557
		// char address_string[INET6_ADDRSTRLEN];
		// // TODO: Think of better name for cast_address
		// void *cast_address;

		// // Not sure when/if I should be casting ot
		// if (address.sa_family == AF_INET)
		// {
		// 	// IPv4
		// 	cast_address = &((struct sockaddr_in *)address.sa_data)->sin_addr;
		// }
		// else
		// {
		// 	// IPv6
		// 	cast_address = &((struct sockaddr_in6 *)address.sa_data)->sin6_addr;
		// }

		// // Network representation to printable string
		// inet_ntop(address.sa_family, cast_address, address_string, INET6_ADDRSTRLEN);

		// printf("Address: %s\n", address_string);

		// accept() blocks until an incoming connection arrives
		// It returns a file descriptor to the connection
		// int client_fd = accept(socket_fd, NULL, NULL);

		// Zero out the receive buffer to make sure it ends up null-terminated
		bzero(received, MAX_RECEIVED_LEN + 1);

		// Read the client's message
		ssize_t bytes_read;
		while ((bytes_read = read(client_fd, received, MAX_RECEIVED_LEN)) > 0)
		{
			printf("\nbin2hex:\n%s\n\nReceived:\n%s", bin2hex(received, bytes_read), received);

			// TODO: This is a hacky way to detect the end of the message
			if (received[bytes_read - 1] == '\n')
			{
				break;
			}

			bzero(received, MAX_RECEIVED_LEN + 1);
		}
		if (bytes_read < 0)
		{
			die("read");
		}

		// Send the response
		// TODO: Don't ignore errors
		write(client_fd, sent, strlen(sent));
		close(client_fd);
	}

	return EXIT_SUCCESS;
}
