#include "ClientData.hpp"

#include <arpa/inet.h>
#include <cassert>
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
#define MAX_CGI_WRITE_LEN 3
#define MAX_CLIENT_WRITE_LEN 2
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

	sockaddr_in servaddr{};
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

	// fd_to_pfds_index.insert(std::make_pair(server_fd, pfds.size()));

	pollfd server_pfd;
	server_pfd.fd = server_fd;
	server_pfd.events = POLLIN;
	pfds.push_back(server_pfd);

	std::cerr << "Port is " << SERVER_PORT << std::endl;

	std::unordered_map<int, size_t> fd_to_client_data_index;

	std::vector<ClientData> client_data;

	std::unordered_map<int, FdType::FdType> fd_to_fd_type;

	fd_to_fd_type.insert(std::make_pair(server_fd, FdType::SERVER));

	while (true)
	{
		std::cerr << "Waiting on an event..." << std::endl;
		fflush(stdout);

		if (poll(pfds.data(), pfds.size(), -1) == -1)
		{
			perror("poll");
			exit(EXIT_FAILURE);
		}

		for (nfds_t pfd_index = pfds.size(); pfd_index > 0;)
		{
			pfd_index--;

			int fd = pfds[pfd_index].fd;
			// std::cerr << "A" << std::endl;
			FdType::FdType fd_type = fd_to_fd_type.at(fd);
			// std::cerr << "B" << std::endl;

			if (pfds[pfd_index].revents != 0)
			{
				std::cerr << "  fd=" << pfds[pfd_index].fd << "; "
						  << "Events: "
						  << ((pfds[pfd_index].revents & POLLIN) ? "POLLIN " : "")
						  << ((pfds[pfd_index].revents & POLLOUT) ? "POLLOUT " : "")
						  << ((pfds[pfd_index].revents & POLLHUP) ? "POLLHUP " : "")
						  << ((pfds[pfd_index].revents & POLLNVAL) ? "POLLNVAL " : "")
						  << ((pfds[pfd_index].revents & POLLPRI) ? "POLLPRI " : "")
						  << ((pfds[pfd_index].revents & POLLRDBAND) ? "POLLRDBAND " : "")
						  << ((pfds[pfd_index].revents & POLLRDNORM) ? "POLLRDNORM " : "")
						  << ((pfds[pfd_index].revents & POLLWRBAND) ? "POLLWRBAND " : "")
						  << ((pfds[pfd_index].revents & POLLWRNORM) ? "POLLWRNORM " : "")
						  << ((pfds[pfd_index].revents & POLLERR) ? "POLLERR " : "")
						  << "(" << std::hex << pfds[pfd_index].revents << ")" << std::endl;

				// TODO: Split these events
				if (pfds[pfd_index].revents & POLLHUP || pfds[pfd_index].revents & POLLERR)
				{
					int client_fd = pfds[pfd_index].fd;

					size_t client_data_index = fd_to_client_data_index.at(client_fd);
					ClientData &client = client_data[client_data_index];

					// std::cerr << "    Closing fd " << client_fd << std::endl;

					fd_to_pfds_index.erase(client_fd);
					fd_to_client_data_index.erase(client_fd);
					if (close(client_fd) == -1)
					{
						perror("close");
						exit(EXIT_FAILURE);
					}
					client.fd = -1;

					if (client.server_to_cgi_fd != -1)
					{
						fd_to_pfds_index.erase(client.server_to_cgi_fd);
						fd_to_client_data_index.erase(client.server_to_cgi_fd);
						if (close(client.server_to_cgi_fd) == -1)
						{
							perror("close");
							exit(EXIT_FAILURE);
						}
						client.server_to_cgi_fd = -1;
					}

					if (client.cgi_to_server_fd != -1)
					{
						fd_to_pfds_index.erase(client.cgi_to_server_fd);
						fd_to_client_data_index.erase(client.cgi_to_server_fd);
						if (close(client.cgi_to_server_fd) == -1)
						{
							perror("close");
							exit(EXIT_FAILURE);
						}
						client.cgi_to_server_fd = -1;
					}

					// Swap-remove
					fd_to_pfds_index[pfds.back().fd] = pfd_index;

					// Swap-remove
					// Explanation: If pfds.size() == 2, with pfd[1] firing POLLIN and pfd[0] firing POLLHUP,
					// pfd[1] will be handled first, and then pfd[0] will run the below two lines, moving pfd[1] to pfds[0].
					// This is fine however, since the for-loop's pfd_index > 0 condition will be false, meaning pfd[1] won't be processed twice.
					//
					// In this visualization, the processed pfd is put in <>:
					// [ POLLHUP_A, POLLHUP_B, <POLLIN> ] -> [ POLLHUP_A, <POLLHUP_B>, POLLIN ] -> [ <POLLHUP_A>, POLLIN ] -> [ POLLIN ]
					pfds[pfd_index] = pfds.back();
					pfds.pop_back();

					// Swap-remove
					fd_to_client_data_index[client_data.back().fd] = client_data_index;

					// Swap-remove
					client_data[client_data_index] = client_data.back();
					client_data.pop_back();

					continue;
				}

				// Can read
				if (pfds[pfd_index].revents & POLLIN || pfds[pfd_index].revents & POLLRDBAND || pfds[pfd_index].revents & POLLRDNORM || pfds[pfd_index].revents & POLLPRI)
				{
					if (fd_type == FdType::SERVER)
					{
						int client_fd = accept(server_fd, NULL, NULL);
						std::cerr << "  Accepted client fd " << client_fd << std::endl;

						fd_to_pfds_index.insert(std::make_pair(client_fd, pfds.size()));

						fd_to_client_data_index.insert(std::make_pair(client_fd, client_data.size()));

						pollfd client_pfd;
						client_pfd.fd = client_fd;
						client_pfd.events = POLLIN;
						pfds.push_back(client_pfd);

						client_data.push_back(ClientData(client_fd));

						fd_to_fd_type.insert(std::make_pair(client_fd, FdType::CLIENT));
					}
					else
					{
						assert(fd_type == FdType::CLIENT || fd_type == FdType::CGI_TO_SERVER);

						int client_fd = pfds[pfd_index].fd;

						size_t client_data_index = fd_to_client_data_index.at(client_fd);
						ClientData &client = client_data[client_data_index];

						ClientReadState::ClientReadState previous_read_state = client.client_read_state;

						if (!client.readSocket(pfds, fd_to_pfds_index, fd_type))
						{
							// TODO: Print error
							exit(EXIT_FAILURE);
						}

						// If we've just started reading this client's body
						if (previous_read_state != ClientReadState::BODY && client.client_read_state == ClientReadState::BODY)
						{
							// TODO: Only run the below code if the request wants to start the CGI

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

								std::cerr << "  Child is going to exec Python" << std::endl;
								// TODO: Define Python path in configuration file?
								const char *path = "/usr/bin/python3";
								char *const argv[] = {(char *)"python3", (char *)"print.py", NULL};

								// TODO: Construct cgi_env using header_map
								char *cgi_env[] = {NULL};
								execve(path, argv, cgi_env);

								perror("execve");
								return EXIT_FAILURE;
							}

							close(server_to_cgi_tube[PIPE_READ_INDEX]);
							// std::cerr << "  Server closed server_to_cgi_tube[PIPE_READ_INDEX] fd " << server_to_cgi_tube[PIPE_READ_INDEX] << std::endl;
							close(cgi_to_server_tube[PIPE_WRITE_INDEX]);
							// std::cerr << "  Server closed cgi_to_server_tube[PIPE_WRITE_INDEX] fd " << cgi_to_server_tube[PIPE_WRITE_INDEX] << std::endl;

							size_t client_data_index = fd_to_client_data_index.at(client.fd);

							int server_to_cgi_fd = server_to_cgi_tube[PIPE_WRITE_INDEX];

							fd_to_pfds_index.insert(std::make_pair(server_to_cgi_fd, pfds.size()));
							pollfd server_to_cgi_pfd;
							server_to_cgi_pfd.fd = server_to_cgi_fd;
							server_to_cgi_pfd.events = client.body.empty() ? 0 : POLLOUT;
							// std::cerr << "  have_read_body is " << client.have_read_body << std::endl;
							pfds.push_back(server_to_cgi_pfd);

							client.server_to_cgi_fd = server_to_cgi_fd;

							fd_to_client_data_index.insert(std::make_pair(server_to_cgi_fd, client_data_index));
							client.cgi_write_state = CGIWriteState::WRITING_TO_CGI;
							fd_to_fd_type.insert(std::make_pair(server_to_cgi_fd, FdType::SERVER_TO_CGI));

							std::cerr << "  Added server_to_cgi fd " << server_to_cgi_fd << std::endl;

							int cgi_to_server_fd = cgi_to_server_tube[PIPE_READ_INDEX];

							fd_to_pfds_index.insert(std::make_pair(cgi_to_server_fd, pfds.size()));
							pollfd cgi_to_server_pfd;
							cgi_to_server_pfd.fd = cgi_to_server_fd;
							cgi_to_server_pfd.events = POLLIN;
							pfds.push_back(cgi_to_server_pfd);

							fd_to_client_data_index.insert(std::make_pair(cgi_to_server_fd, client_data_index));
							client.cgi_read_state = CGIReadState::READING_FROM_CGI;
							fd_to_fd_type.insert(std::make_pair(cgi_to_server_fd, FdType::CGI_TO_SERVER));

							std::cerr << "  Added cgi_to_server fd " << cgi_to_server_fd << std::endl;
						}
					}
				}

				// Can write
				if (pfds[pfd_index].revents & POLLOUT || pfds[pfd_index].revents & POLLWRBAND || pfds[pfd_index].revents & POLLWRNORM)
				{
					int client_fd = pfds[pfd_index].fd;

					size_t client_data_index = fd_to_client_data_index.at(client_fd);
					ClientData &client = client_data[client_data_index];

					if (fd_type == FdType::SERVER_TO_CGI)
					{
						// TODO: Remove this before the evaluation
						assert(client.cgi_write_state == CGIWriteState::WRITING_TO_CGI);

						size_t max_cgi_write_len = MAX_CGI_WRITE_LEN; // TODO: Read from config
						size_t body_substr_len = std::min(client.body.length() - client.body_index, max_cgi_write_len);

						std::cerr << "body_substr_len is " << body_substr_len << " with a body length of " << client.body.length() << " and as body '" << client.body << "'" << std::endl;

						// TODO: Remove this before the evaluation
						assert(body_substr_len > 0);

						// TODO: substr() can fail
						std::string body_substr = client.body.substr(client.body_index, body_substr_len);

						client.body_index += body_substr_len;

						std::cerr << "    Sending this body substr to the CGI that has a len of " << body_substr.length() << ":" << std::endl
								<< "'" << body_substr << "'" << std::endl;

						// TODO: Don't ignore errors
						write(client.server_to_cgi_fd, body_substr.c_str(), body_substr.length());

						std::cerr << "    Closing server_to_cgi fd " << client.server_to_cgi_fd << std::endl;


						// TODO: Close
						// TODO: Don't *always* close right after a single write


						// If we don't have anything left to write at the moment
						if (client.body_index == client.body.length())
						{
							// Disable POLLOUT
							pfds[pfd_index].events &= ~POLLOUT;
						}
					}
					else if (fd_type == FdType::CLIENT)
					{
						// TODO: Remove this before the evaluation
						assert(client.client_write_state == ClientWriteState::WRITING_TO_CLIENT);

						size_t max_client_write_len = MAX_CLIENT_WRITE_LEN; // TODO: Read from config
						size_t response_substr_len = std::min(client.response.length() - client.response_index, max_client_write_len);

						// if (response_substr_len == 0)
						// {
						// 	continue;
						// }

						// TODO: Remove this before the evaluation
						assert(response_substr_len > 0);

						// TODO: substr() can fail
						std::string response_substr = client.response.substr(client.response_index, response_substr_len);

						client.response_index += response_substr_len;

						std::cerr << "    Sending this response substr that has a len of " << response_substr.length() << ":" << std::endl
								<< "'" << response_substr << "'" << std::endl;

						// TODO: Don't ignore errors
						write(client_fd, response_substr.c_str(), response_substr.length());

						std::cerr << "    Closing client fd " << client_fd << std::endl;


						// TODO: Close
						// TODO: Don't *always* close right after a single write


						// fd_to_pfds_index.erase(client_fd);
						// fd_to_client_data_index.erase(client_fd);
						// if (close(client_fd) == -1)
						// {
						// 	perror("close");
						// 	exit(EXIT_FAILURE);
						// }
						// client.fd = -1;

						// fd_to_pfds_index[pfds.back().fd] = pfd_index;

						// // Swap-remove explanation: If pfds.size() == 2, with pfd[1] firing POLLIN and pfd[0] firing POLLHUP,
						// // pfd[1] will be handled first, and then pfd[0] will run the below two lines, moving pfd[1] to pfds[0].
						// // This is fine however, since the for-loop's pfd_index > 0 condition will be false, meaning pfd[1] won't be processed twice.
						// //
						// // In this visualization, the processed pfd is put in <>:
						// // [ POLLHUP_A, POLLHUP_B, <POLLIN> ] -> [ POLLHUP_A, <POLLHUP_B>, POLLIN ] -> [ <POLLHUP_A>, POLLIN ] -> [ POLLIN ]
						// pfds[pfd_index] = pfds.back();
						// pfds.pop_back();

						// If we don't have anything left to write at the moment
						if (client.response_index == client.response.length())
						{
							// Disable POLLOUT
							pfds[pfd_index].events &= ~POLLOUT;
						}
					}
					else
					{
						// Should be unreachable
						// TODO: Remove this before the evaluation
						assert(false);
					}
				}

				// This can be reached by commenting out the line that removes an fd from pfds above
				if (pfds[pfd_index].revents & POLLNVAL)
				{
					// TODO: Print error
					exit(EXIT_FAILURE);
				}
			}
		}
	}

	return EXIT_SUCCESS;
}
