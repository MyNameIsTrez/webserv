#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// TODO: Move some/all of these defines to a config file
#define SERVER_PORT 18000

#define MAX_CONNECTION_QUEUE_LENGTH 1

#define MAX_RECEIVED_LEN 300

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
	// The protocol 0 lets socket() pick a protocol, based on the requested socket type (stream)
	// Source: https://pubs.opengroup.org/onlinepubs/009695399/functions/socket.html
	int server_fd;
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket");
		exit(EXIT_FAILURE);
	}

	// Enables local address reuse
	// This prevents socket() calls from possibly returning an error on server restart
	int option = 1; // "the parameter should be non-zero to enable a boolean option"
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERVER_PORT);

	if ((bind(server_fd, (struct sockaddr *)&servaddr, sizeof(servaddr))) < 0)
	{
		perror("bind");
		exit(EXIT_FAILURE);
	}

	if ((listen(server_fd, MAX_CONNECTION_QUEUE_LENGTH)) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	nfds_t nfds = 2;

	struct pollfd *poll_fds = calloc(nfds, sizeof(struct pollfd));
	if (poll_fds == NULL)
	{
		perror("calloc");
		exit(EXIT_FAILURE);
	}

	poll_fds[0].fd = server_fd;
	poll_fds[0].events = POLLIN;

	poll_fds[1].fd = -1;

	char received[MAX_RECEIVED_LEN + 1];
	bzero(received, MAX_RECEIVED_LEN + 1);

	char *sent = "HTTP/1.0 200 OK\r\n\r\n<h1>Hello</h1><p>World</p>";

	printf("Port is %d\n\n", SERVER_PORT);

	while (true)
	{
		printf("Waiting on an event...\n");
		if (poll(poll_fds, nfds, -1) == -1)
		{
			perror("poll");
			exit(EXIT_FAILURE);
		}

		for (nfds_t j = 0; j < nfds; j++)
		{
			if (poll_fds[j].revents != 0)
			{
				printf("  fd=%d; events: %s%s%s\n", poll_fds[j].fd,
					   (poll_fds[j].revents & POLLIN) ? "POLLIN " : "",
					   (poll_fds[j].revents & POLLHUP) ? "POLLHUP " : "",
					   (poll_fds[j].revents & POLLERR) ? "POLLERR " : "");

				if (poll_fds[j].revents & POLLIN)
				{
					if (poll_fds[j].fd == server_fd)
					{
						if (poll_fds[1].fd != -1)
						{
							close(poll_fds[1].fd);
						}

						int client_fd = accept(server_fd, NULL, NULL);
						printf("  Accepted client fd %d\n\n", client_fd);

						poll_fds[1].fd = client_fd;
						poll_fds[1].events = POLLIN;
					}
					else
					{
						int client_fd = poll_fds[j].fd;

						ssize_t bytes_read = read(client_fd, received, MAX_RECEIVED_LEN);
						if (bytes_read == -1)
						{
							perror("read");
							exit(EXIT_FAILURE);
						}

						printf("\nRead %zd bytes:\n'%s'\n\nIn hexit form:\n%s\n\n\n", bytes_read, received, bin2hex(received, bytes_read));

						// TODO: This is an extremely buggy way to detect the end of the message
						if (received[bytes_read - 1] == '\n')
						{
							printf("Sending this response:\n'%s'\nAnd closing the client fd %d\n\n", sent, client_fd);
							// TODO: Don't ignore errors
							write(client_fd, sent, strlen(sent));
							close(client_fd);
							poll_fds[j].fd = -1;
						}

						bzero(received, MAX_RECEIVED_LEN + 1);
					}
				}
				else
				{ // POLLERR or POLLHUP
					printf("    closing fd %d\n", poll_fds[j].fd);

					if (close(poll_fds[j].fd) == -1)
					{
						perror("close");
						exit(EXIT_FAILURE);
					}
				}
			}
		}
	}

	return EXIT_SUCCESS;
}
