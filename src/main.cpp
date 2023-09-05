#include "ClientData.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

// TODO: Move some/all of these defines to a config file
#define SERVER_PORT 18000
#define MAX_CONNECTION_QUEUE_LEN 10
#define MAX_RESPONSE_LEN 2
#define CHILD 0
#define PIPE_READ_INDEX 0
#define PIPE_WRITE_INDEX 1

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

	sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERVER_PORT);

	if ((bind(server_fd, (sockaddr *)&servaddr, sizeof(servaddr))) < 0)
	{
		perror("bind");
		exit(EXIT_FAILURE);
	}

	if ((listen(server_fd, MAX_CONNECTION_QUEUE_LEN)) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	std::vector<pollfd> pfds;

	std::unordered_map<int, size_t> fd_to_pfds_index;

	fd_to_pfds_index.insert(std::make_pair(server_fd, pfds.size()));

	pollfd server_pfd;
	server_pfd.fd = server_fd;
	server_pfd.events = POLLIN;
	pfds.push_back(server_pfd);

	std::cerr << "Port is " << SERVER_PORT << std::endl;

	// TODO: Should the server be pushed into here as well?
	std::unordered_map<int, ClientData> client_data;

	while (true)
	{
		std::cerr << "Waiting on an event..." << std::endl;
		fflush(stdout);

		if (poll(pfds.data(), pfds.size(), -1) == -1)
		{
			perror("poll");
			exit(EXIT_FAILURE);
		}

		for (nfds_t j = pfds.size(); j > 0;)
		{
			j--;

			if (pfds[j].revents != 0)
			{
				std::cerr << "  fd=" << pfds[j].fd << "; "
						  << "Events: "
						  << ((pfds[j].revents & POLLIN) ? "POLLIN " : "")
						  << ((pfds[j].revents & POLLOUT) ? "POLLOUT " : "")
						  << ((pfds[j].revents & POLLHUP) ? "POLLHUP " : "")
						  << ((pfds[j].revents & POLLNVAL) ? "POLLNVAL " : "")
						  << ((pfds[j].revents & POLLPRI) ? "POLLPRI " : "")
						  << ((pfds[j].revents & POLLRDBAND) ? "POLLRDBAND " : "")
						  << ((pfds[j].revents & POLLRDNORM) ? "POLLRDNORM " : "")
						  << ((pfds[j].revents & POLLWRBAND) ? "POLLWRBAND " : "")
						  << ((pfds[j].revents & POLLWRNORM) ? "POLLWRNORM " : "")
						  << ((pfds[j].revents & POLLERR) ? "POLLERR " : "")
						  << "(" << std::hex << pfds[j].revents << ")" << std::endl;

				// TODO: Split these events
				if (pfds[j].revents & POLLHUP || pfds[j].revents & POLLERR)
				{
					int client_fd = pfds[j].fd;

					std::cerr << "    Closing fd " << client_fd << std::endl;

					client_data.erase(client_fd);
					if (close(client_fd) == -1)
					{
						perror("close");
						exit(EXIT_FAILURE);
					}

					fd_to_pfds_index[pfds.back().fd] = j;

					// Swap-remove explanation: If pfds.size() == 2, with pfd[1] firing POLLIN and pfd[0] firing POLLHUP,
					// pfd[1] will be handled first, and then pfd[0] will run the below two lines, moving pfd[1] to pfds[0].
					// This is fine however, since the for-loop's j > 0 condition will be false, meaning pfd[1] won't be processed twice.
					//
					// In this visualization, the processed pfd is put in <>:
					// [ POLLHUP_A, POLLHUP_B, <POLLIN> ] -> [ POLLHUP_A, <POLLHUP_B>, POLLIN ] -> [ <POLLHUP_A>, POLLIN ] -> [ POLLIN ]
					pfds[j] = pfds.back();
					pfds.pop_back();

					continue;
				}

				// Can read
				if (pfds[j].revents & POLLIN || pfds[j].revents & POLLRDBAND || pfds[j].revents & POLLRDNORM || pfds[j].revents & POLLPRI)
				{
					if (pfds[j].fd == server_fd)
					{
						int client_fd = accept(server_fd, NULL, NULL);
						std::cerr << "  Accepted client fd " << client_fd << std::endl;

						fd_to_pfds_index.insert(std::make_pair(client_fd, pfds.size()));

						pollfd client_pfd;
						client_pfd.fd = client_fd;
						client_pfd.events = POLLIN;
						pfds.push_back(client_pfd);

						client_data.insert(std::make_pair(client_fd, ClientData(client_fd)));
					}
					else
					{
						int client_fd = pfds[j].fd;
						// TODO: "client" can dangle if the map decides to rearrange its data (growing, for example)
						ClientData &client = client_data[client_fd];

						ReadState::ReadState previous_read_state = client.read_state;

						if (!client.readSocket(pfds, fd_to_pfds_index))
						{
							// TODO: Print error
							exit(EXIT_FAILURE);
						}

						// If we've just started reading this client's body
						if (previous_read_state != ReadState::BODY && client.read_state == ReadState::BODY)
						{
							// std::cerr << "foo" << std::endl;

							// Turn on the POLLOUT bit
							// pfds[j].events |= POLLOUT;
							pfds[j].events = 0;

							// TODO: Only run the below code if the request says it wants to start the CGI

							int server_to_cgi_tube[2];
							int cgi_to_server_tube[2];

							if (pipe(server_to_cgi_tube) == -1)
							{
								perror("pipe");
								return EXIT_FAILURE;
							}
							if (pipe(cgi_to_server_tube) == -1)
							{
								perror("pipe");
								return EXIT_FAILURE;
							}

							pid_t forked_pid = fork();
							if (forked_pid == -1)
							{
								perror("fork");
								return EXIT_FAILURE;
							}
							else if (forked_pid == CHILD)
							{
								close(server_to_cgi_tube[PIPE_WRITE_INDEX]);
								close(cgi_to_server_tube[PIPE_READ_INDEX]);

								if (dup2(server_to_cgi_tube[PIPE_READ_INDEX], STDIN_FILENO) == -1)
								{
									perror("dup2");
									return EXIT_FAILURE;
								}
								close(server_to_cgi_tube[PIPE_READ_INDEX]);

								if (dup2(cgi_to_server_tube[PIPE_WRITE_INDEX], STDOUT_FILENO) == -1)
								{
									perror("dup2");
									return EXIT_FAILURE;
								}
								close(cgi_to_server_tube[PIPE_WRITE_INDEX]);

								fprintf(stderr, "Child is going to exec Python\n");
								// TODO: Define Python path in configuration file?
								const char *path = "/usr/local/bin/python3";
								char *const argv[] = {(char *)"python3", (char *)"print.py", NULL};

								// TODO: Construct cgi_env using header_map
								char *cgi_env[] = {NULL};
								execve(path, argv, cgi_env);

								perror("execve");
								return EXIT_FAILURE;
							}

							close(server_to_cgi_tube[PIPE_READ_INDEX]);
							std::cerr << "  Server closed server_to_cgi_tube[PIPE_READ_INDEX] fd " << server_to_cgi_tube[PIPE_READ_INDEX] << std::endl;
							close(cgi_to_server_tube[PIPE_WRITE_INDEX]);
							std::cerr << "  Server closed cgi_to_server_tube[PIPE_WRITE_INDEX] fd " << cgi_to_server_tube[PIPE_WRITE_INDEX] << std::endl;

							int server_to_cgi_fd = server_to_cgi_tube[PIPE_WRITE_INDEX];

							fd_to_pfds_index.insert(std::make_pair(server_to_cgi_fd, pfds.size()));

							pollfd server_to_cgi_pfd;
							server_to_cgi_pfd.fd = server_to_cgi_fd;
							server_to_cgi_pfd.events = client.have_read_body ? POLLOUT : 0;
							pfds.push_back(server_to_cgi_pfd);

							ClientData server_to_cgi_data(server_to_cgi_fd);
							server_to_cgi_data.read_state = ReadState::NOT_READING;
							server_to_cgi_data.write_state = WriteState::WRITING_TO_CGI;
							client_data.insert(std::make_pair(server_to_cgi_fd, server_to_cgi_data));

							std::cerr << "  Added server_to_cgi fd " << server_to_cgi_fd << std::endl;

							int cgi_to_server_fd = cgi_to_server_tube[PIPE_READ_INDEX];

							fd_to_pfds_index.insert(std::make_pair(cgi_to_server_fd, pfds.size()));

							pollfd cgi_to_server_pfd;
							cgi_to_server_pfd.fd = cgi_to_server_fd;
							cgi_to_server_pfd.events = POLLIN;
							pfds.push_back(cgi_to_server_pfd);

							ClientData cgi_to_server_data(cgi_to_server_fd);
							cgi_to_server_data.read_state = ReadState::READING_FROM_CGI;
							cgi_to_server_data.write_state = WriteState::NOT_WRITING;
							client_data.insert(std::make_pair(cgi_to_server_fd, cgi_to_server_data));

							std::cerr << "  Added cgi_to_server fd " << cgi_to_server_fd << std::endl;
						}
						else if (client.read_state == ReadState::DONE)
						{
							// Turn off the POLLIN bit
							// Not strictly necessary, since POLLIN can't be raised after read() returned 0 anyways
							pfds[j].events &= ~POLLIN;
						}
					}
				}

				// Can write
				if (pfds[j].revents & POLLOUT || pfds[j].revents & POLLWRBAND || pfds[j].revents & POLLWRNORM)
				{
					int client_fd = pfds[j].fd;
					// TODO: "client" can dangle if the map decides to rearrange its data (growing, for example)
					ClientData &client = client_data[client_fd];

					size_t max_response_len = MAX_RESPONSE_LEN; // TODO: Read from config
					size_t response_substr_len = std::min(client.response.length() - client.response_index, max_response_len);

					// if (response_substr_len == 0)
					// {
					// 	continue;
					// }

					// TODO: Remove this before the evaluation
					assert(response_substr_len > 0);

					std::string response_substr = client.response.substr(client.response_index, response_substr_len);

					std::cerr << "    Sending this response substr that has a len of " << response_substr.length() << ":" << std::endl
							  << "'" << response_substr << "'" << std::endl;

					// TODO: Don't *always* close right after a single write
					// TODO: Don't ignore errors
					write(client_fd, response_substr.c_str(), response_substr_len);

					std::cerr << "    Closing fd " << client_fd << std::endl;

					client_data.erase(client_fd);
					if (close(client_fd) == -1)
					{
						perror("close");
						exit(EXIT_FAILURE);
					}

					fd_to_pfds_index[pfds.back().fd] = j;

					// Swap-remove explanation: If pfds.size() == 2, with pfd[1] firing POLLIN and pfd[0] firing POLLHUP,
					// pfd[1] will be handled first, and then pfd[0] will run the below two lines, moving pfd[1] to pfds[0].
					// This is fine however, since the for-loop's j > 0 condition will be false, meaning pfd[1] won't be processed twice.
					//
					// In this visualization, the processed pfd is put in <>:
					// [ POLLHUP_A, POLLHUP_B, <POLLIN> ] -> [ POLLHUP_A, <POLLHUP_B>, POLLIN ] -> [ <POLLHUP_A>, POLLIN ] -> [ POLLIN ]
					pfds[j] = pfds.back();
					pfds.pop_back();

					continue;
				}

				// This can be reached by commenting out the line that removes an fd from pfds above
				if (pfds[j].revents & POLLNVAL)
				{
					// TODO: Print error
					exit(EXIT_FAILURE);
				}
			}
		}
	}

	return EXIT_SUCCESS;
}
