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
#include <map>
#include <string>
#include <vector>
#include <iostream>

#include "ClientData.hpp"

// TODO: Move some/all of these defines to a config file
#define SERVER_PORT 18000
#define MAX_CONNECTION_QUEUE_LENGTH 10

// TODO: Use this on every function that can fail
void die()
{
	// TODO: Loop over all fds and close them

	// TODO: We are not allowed to use perror(), but we can use strerror()

	exit(EXIT_FAILURE);
}

// c++ main.cpp ClientData.cpp -Wall -Wextra -Werror -Wpedantic -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
// Code stolen from https://youtu.be/esXw4bdaZkc
int main(void)
{
	// TODO: Parse config

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
	// "the parameter should be non-zero to enable a boolean option"
	int option = 1;
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

	std::vector<struct pollfd> poll_fds;

	// Create server struct
	struct pollfd pfd;
	pfd.fd = server_fd;
	pfd.events = POLLIN;
	poll_fds.push_back(pfd);

	// const char *sent = "HTTP/1.0 200 OK\r\n\r\n<h1>Hello</h1><p>World</p>";

	std::cerr << "Port is " << SERVER_PORT << std::endl;

	std::map<int, ClientData> client_data;

	while (true)
	{
		std::cerr << "Waiting on an event..." << std::endl;
		fflush(stdout);

		if (poll(poll_fds.data(), poll_fds.size(), -1) == -1)
		{
			perror("poll");
			exit(EXIT_FAILURE);
		}

		for (nfds_t j = poll_fds.size(); j > 0;)
		{
			j--;

			if (poll_fds[j].revents != 0)
			{
				std::cerr << "  fd=" << poll_fds[j].fd << "; "
						  << "events: "
						  << ((poll_fds[j].revents & POLLIN) ? "POLLIN " : "")
						  << ((poll_fds[j].revents & POLLOUT) ? "POLLOUT " : "")
						  << ((poll_fds[j].revents & POLLHUP) ? "POLLHUP " : "")
						  << ((poll_fds[j].revents & POLLNVAL) ? "POLLNVAL " : "")
						  << ((poll_fds[j].revents & POLLPRI) ? "POLLPRI " : "")
						  << ((poll_fds[j].revents & POLLRDBAND) ? "POLLRDBAND " : "")
						  << ((poll_fds[j].revents & POLLRDNORM) ? "POLLRDNORM " : "")
						  << ((poll_fds[j].revents & POLLWRBAND) ? "POLLWRBAND " : "")
						  << ((poll_fds[j].revents & POLLWRNORM) ? "POLLWRNORM " : "")
						  << ((poll_fds[j].revents & POLLERR) ? "POLLERR " : "")
						  << "(" << std::hex << poll_fds[j].revents << ")" << std::endl;

				// TODO: Split these events
				if (poll_fds[j].revents & POLLHUP || poll_fds[j].revents & POLLERR)
				{
					std::cerr << "    closing fd " << poll_fds[j].fd << std::endl;

					client_data.erase(poll_fds[j].fd);
					if (close(poll_fds[j].fd) == -1)
					{
						perror("close");
						exit(EXIT_FAILURE);
					}

					poll_fds[j] = poll_fds.back();
					poll_fds.pop_back();

					continue;
				}
				// Can read
				if (poll_fds[j].revents & POLLIN || poll_fds[j].revents & POLLRDBAND || poll_fds[j].revents & POLLRDNORM || poll_fds[j].revents & POLLPRI)
				{
					if (poll_fds[j].fd == server_fd)
					{
						int client_fd = accept(server_fd, NULL, NULL);
						std::cerr << "  Accepted client fd " << client_fd << std::endl
								  << std::endl;

						struct pollfd pfd;
						pfd.fd = client_fd;
						pfd.events = POLLIN;
						poll_fds.push_back(pfd);

						client_data.insert(std::pair<int, ClientData>(client_fd, ClientData(client_fd)));
					}
					else
					{
						int client_fd = poll_fds[j].fd;
						client_data[client_fd].readSocket();

						if (client_data[client_fd].state == ClientData::BODY)
						{
							// Turn on POLLOUT bit
							// TODO: Find a way to only do this once
							poll_fds[j].events |= POLLOUT;
						}
						// else if (client_data[client_fd].state == ClientData::DONE)
						// {
						// 	// Turn off POLLIN bit
						// 	poll_fds[j].events &= ~POLLIN;
						// }
					}
				}
				// Can write
				if (poll_fds[j].revents & POLLOUT || poll_fds[j].revents & POLLWRBAND || poll_fds[j].revents & POLLWRNORM)
				{
				}
				// This can be reached by commenting out the line that removes an fd from poll_fds above
				if (poll_fds[j].revents & POLLNVAL)
				{
					// TODO: Maybe print something?
					exit(EXIT_FAILURE);
				}
			}
		}
	}

	return EXIT_SUCCESS;
}
