#include <errno.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_RECEIVED_LEN 4096
#define MAX_SENT_LEN 4096

#define SERVER_PORT 18000

#define MAX_CONNECTION_QUEUE_LENGTH 10

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

char *bin2hex(uint8_t *input, size_t len)
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
	if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		die("socket");
	}

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

	uint8_t received[MAX_RECEIVED_LEN + 1];
	uint8_t sent[MAX_SENT_LEN + 1];

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
		// struct sockaddr address;
		// socklen_t address_len;
		// client_fd = accept(socket_fd, &address, &address_len);

		// accept() blocks until an incoming connection arrives
		// It returns a file descriptor to the connection
		int client_fd = accept(socket_fd, NULL, NULL);

		// Zero out the receive buffer to make sure it ends up null-terminated
		bzero(received, MAX_RECEIVED_LEN + 1);

		// Read the client's message
		ssize_t bytes_read;
		while ((bytes_read = read(client_fd, received, MAX_RECEIVED_LEN)) > 0)
		{
			printf("\nbin2hex: %s\n\nreceived: %s", bin2hex(received, bytes_read), received);

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

		// Send a response
		snprintf((char *)sent, sizeof(sent), "HTTP/1.0 200 OK\r\n\r\nHello");

		// TODO: Don't ignore errors
		write(client_fd, sent, strlen((char *)sent));
		close(client_fd);
	}

	return EXIT_SUCCESS;
}
