#include "Client.hpp"

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
#include <sys/types.h>
#include <sys/wait.h>
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

// TODO: Turn these into static ints inside of a class?
const int POLLIN_ANY = POLLIN | POLLRDBAND | POLLRDNORM | POLLPRI;
const int POLLOUT_ANY = POLLOUT | POLLWRBAND | POLLWRNORM;

// TODO: Use this on every function that can fail
// static void die()
// {
// 	// TODO: Loop over all fds and close them

// 	// TODO: We are not allowed to use perror(), but we can use strerror()

// 	exit(EXIT_FAILURE);
// }

// This is called in many spots
// due to the fact that client pointer can dangle
// whenever the clients vector resizes
static Client &getClient(const std::unordered_map<int, size_t> &fd_to_client_index, int fd, std::vector<Client> &clients)
{
	size_t client_index = fd_to_client_index.at(fd);
	return clients[client_index];
}

// Source: https://stackoverflow.com/a/22940622/13279557
static void sigChildHandler(int signum)
{
	pid_t child_pid;
	int child_exit_status;

	(void)signum;

	// Reaps all children that have exited
	// waitpid() returns 0 if no more children can be reaped right now
	// WNOHANG guarantees that this call doesn't block
	while ((child_pid = waitpid(-1, &child_exit_status, WNOHANG)) > 0)
	{
		if (WIFEXITED(child_exit_status))
		{
			std::cerr << "PID " << child_pid << " exited normally. Exit status: " << WEXITSTATUS(child_exit_status) << std::endl;
		}
		else if (WIFSTOPPED(child_exit_status))
		{
			std::cerr << "PID " << child_pid << " was stopped by " << WSTOPSIG(child_exit_status) << std::endl;
		}
		else if (WIFSIGNALED(child_exit_status))
		{
			std::cerr << "PID " << child_pid << " exited due to signal " << WTERMSIG(child_exit_status) << std::endl;
		}
		else
		{
			// TODO: Decide whether we want to check the remaining WCOREDUMP() and WIFCONTINUED()
			// TODO: ??
			assert(false);
		}
	}
	// TODO: Decide what to do when errno is EINTR
	// errno is set to ECHILD when there are no children left to wait for
	if (child_pid == -1 && errno != ECHILD)
	{
		perror("waitpid");
		exit(EXIT_FAILURE);
	}
}

// c++ main.cpp Client.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
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

	std::unordered_map<int, size_t> fd_to_pfd_index;

	// fd_to_pfd_index.insert(std::make_pair(server_fd, pfds.size()));

	pollfd server_pfd;
	server_pfd.fd = server_fd;
	server_pfd.events = POLLIN;
	pfds.push_back(server_pfd);

	std::cerr << "Port is " << SERVER_PORT << std::endl;

	std::unordered_map<int, size_t> fd_to_client_index;

	std::vector<Client> clients;

	std::unordered_map<int, FdType::FdType> fd_to_fd_type;

	fd_to_fd_type.insert(std::make_pair(server_fd, FdType::SERVER));

	signal(SIGCHLD, sigChildHandler);

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

			if (pfds[pfd_index].revents != 0)
			{
				int fd = pfds[pfd_index].fd;
				FdType::FdType fd_type = fd_to_fd_type.at(fd);

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
						  << std::endl;

				// This can be reached by commenting out a line that removes a closed fd from pfds
				if (pfds[pfd_index].revents & POLLNVAL)
				{
					// Should be unreachable
					// TODO: Remove this before the evaluation
					assert(false);
				}

				// If there was an error, remove the client, and close all its file descriptors
				if (pfds[pfd_index].revents & POLLERR)
				{
					Client &client = getClient(fd_to_client_index, fd, clients);

					if (close(client.client_fd) == -1)
					{
						perror("close");
						exit(EXIT_FAILURE);
					}
					client.client_fd = -1;

					// Close and remove server_to_cgi
					if (client.server_to_cgi_fd != -1)
					{
						// Swap-remove
						size_t server_to_cgi_pfd_index = fd_to_pfd_index.at(client.server_to_cgi_fd);
						fd_to_pfd_index[pfds.back().fd] = server_to_cgi_pfd_index;
						pfds[server_to_cgi_pfd_index] = pfds.back();
						pfds.pop_back();

						fd_to_pfd_index.erase(client.server_to_cgi_fd);
						fd_to_client_index.erase(client.server_to_cgi_fd);

						if (close(client.server_to_cgi_fd) == -1)
						{
							perror("close");
							exit(EXIT_FAILURE);
						}
						client.server_to_cgi_fd = -1;
					}

					// Close and remove cgi_to_server_fd
					if (client.cgi_to_server_fd != -1)
					{
						// Swap-remove
						size_t cgi_to_server_pfd_index = fd_to_pfd_index.at(client.cgi_to_server_fd);
						fd_to_pfd_index[pfds.back().fd] = cgi_to_server_pfd_index;
						pfds[cgi_to_server_pfd_index] = pfds.back();
						pfds.pop_back();

						fd_to_pfd_index.erase(client.cgi_to_server_fd);
						fd_to_client_index.erase(client.cgi_to_server_fd);

						if (close(client.cgi_to_server_fd) == -1)
						{
							perror("close");
							exit(EXIT_FAILURE);
						}
						client.cgi_to_server_fd = -1;
					}

					// Swap-remove
					fd_to_pfd_index.erase(client.client_fd);
					fd_to_pfd_index[pfds.back().fd] = pfd_index;
					// Explanation: If pfds.size() == 2, with pfd[1] firing POLLIN and pfd[0] firing POLLHUP,
					// pfd[1] will be handled first, and then pfd[0] will run the below two lines, moving pfd[1] to pfds[0].
					// This is fine however, since the for-loop's pfd_index > 0 condition will be false, meaning pfd[1] won't be processed twice.
					//
					// In this visualization, the processed pfd is put in <>:
					// [ POLLHUP_A, POLLHUP_B, <POLLIN> ] -> [ POLLHUP_A, <POLLHUP_B>, POLLIN ] -> [ <POLLHUP_A>, POLLIN ] -> [ POLLIN ]
					pfds[pfd_index] = pfds.back();
					pfds.pop_back();

					// Swap-remove
					// TODO: Is it possible for client.client_fd to have already been closed and erased?
					size_t client_index = fd_to_client_index.at(client.client_fd);
					fd_to_client_index.erase(client.client_fd);
					fd_to_client_index[clients.back().client_fd] = client_index;
					clients[client_index] = clients.back();
					clients.pop_back();

					continue;
				}

				// if (pfds[pfd_index].revents & POLLHUP)
				// {
				// 	if (fd_type == FdType::SERVER_TO_CGI || fd_type == FdType::CGI_TO_SERVER)
				// 	{
				// 		Client &client = getClient(fd_to_client_index, fd, clients);

				// 		if (!client.cgi_reaped)
				// 		{
				// 			// If server_to_cgi received a HUP, it means that the Python script exited
				// 			// TODO: Or can it also just mean that the script read all it could from stdin/closed its stdin?
				// 			if (fd_type == FdType::SERVER_TO_CGI && !client.server_to_cgi_hangup)
				// 			{
				// 				client.server_to_cgi_hangup = true;

				// 				client.client_read_state = ClientReadState::DONE;
				// 				size_t client_pfd_index = fd_to_pfd_index.at(client.client_fd);
				// 				std::cerr << "    Disabling client POLLIN" << std::endl;
				// 				pfds[client_pfd_index].events &= ~POLLIN;

				// 				client.cgi_write_state = CGIWriteState::DONE;
				// 				size_t server_to_cgi_pfd_index = fd_to_pfd_index.at(client.server_to_cgi_fd);
				// 				std::cerr << "    Disabling server_to_cgi POLLOUT" << std::endl;
				// 				pfds[server_to_cgi_pfd_index].events &= ~POLLOUT;
				// 			}
				// 			if (fd_type == FdType::CGI_TO_SERVER && !client.cgi_to_server_hangup)
				// 			{
				// 				client.cgi_to_server_hangup = true;
				// 			}

				// 			// One can't assume that when cgi_to_server/server_to_cgi/both get POLLHUP, the Python script exited.
				// 			// This is because the Python script might have manually closed its stdin and stdout,
				// 			// but we still need the exit status of the script for the response sent back to the client.

				// 			int child_exit_status;
				// 			// WNOHANG guarantees that this call doesn't block
				// 			pid_t waited_pid = waitpid(client.cgi_pid, &child_exit_status, WNOHANG);

				// 			if (waited_pid == -1)
				// 			{
				// 				// Should be unreachable
				// 				assert(false);
				// 			}
				// 			// If the CGI has exited
				// 			if (waited_pid != 0)
				// 			{
				// 				client.cgi_reaped = true;

				// 				assert(client.client_read_state == ClientReadState::DONE);
				// 				assert(client.cgi_write_state == CGIWriteState::DONE);
				// 				assert(client.cgi_read_state == CGIReadState::DONE);

				// 				size_t client_pfd_index = fd_to_pfd_index.at(client.client_fd);
				// 				std::cerr << "    Enabling client POLLOUT" << std::endl;
				// 				pfds[client_pfd_index].events |= POLLOUT;
				// 			}
				// 		}
				// 	}
				// }

				// If the other end hung up (closed)
				if (pfds[pfd_index].revents & POLLHUP)
				{
					// If the Python script closed its stdin
					if (fd_type == FdType::SERVER_TO_CGI)
					{
						// TODO: ??
						assert(false);

						// Client &client = getClient(fd_to_client_index, fd, clients);

						// client.client_read_state = ClientReadState::DONE;

						// size_t client_pfd_index = fd_to_pfd_index.at(client.client_fd);
						// std::cerr << "    Disabling client POLLIN" << std::endl;
						// pfds[client_pfd_index].events &= ~POLLIN;

						// client.cgi_write_state = CGIWriteState::DONE;
						// size_t server_to_cgi_pfd_index = fd_to_pfd_index.at(client.server_to_cgi_fd);
						// std::cerr << "    Disabling server_to_cgi POLLOUT" << std::endl;
						// pfds[server_to_cgi_pfd_index].events &= ~POLLOUT;

						// continue;
					}
					// If the Python script closed its stdout
					else if (fd_type == FdType::CGI_TO_SERVER)
					{
						// If the server has read everything from the Python script
						if (!(pfds[pfd_index].revents & POLLIN))
						{
							Client &client = getClient(fd_to_client_index, fd, clients);

							// Disable client POLLIN
							// We don't care that we could've read some more of the client's body
							if (client.client_read_state != ClientReadState::DONE)
							{
								client.client_read_state = ClientReadState::DONE;

								size_t client_pfd_index = fd_to_pfd_index.at(client.client_fd);
								std::cerr << "    Disabling client POLLIN" << std::endl;
								pfds[client_pfd_index].events &= ~POLLIN;
							}

							// Close and remove server_to_cgi
							// We don't care that we could've written some more of the client's body
							if (client.server_to_cgi_fd != -1)
							{
								client.cgi_write_state = CGIWriteState::DONE;

								// Swap-remove
								std::cerr << "    Removing server_to_cgi from pfds" << std::endl;
								size_t server_to_cgi_pfd_index = fd_to_pfd_index.at(client.server_to_cgi_fd);
								fd_to_pfd_index[pfds.back().fd] = server_to_cgi_pfd_index;
								pfds[server_to_cgi_pfd_index] = pfds.back();
								pfds.pop_back();

								fd_to_pfd_index.erase(client.server_to_cgi_fd);
								fd_to_client_index.erase(client.server_to_cgi_fd);

								if (close(client.server_to_cgi_fd) == -1)
								{
									perror("close");
									exit(EXIT_FAILURE);
								}
								client.server_to_cgi_fd = -1;
							}

							// Close and remove cgi_to_server
							{
								assert(client.cgi_to_server_fd != -1);
								client.cgi_read_state = CGIReadState::DONE;

								// Swap-remove
								std::cerr << "    Removing cgi_to_server from pfds" << std::endl;
								size_t cgi_to_server_pfd_index = fd_to_pfd_index.at(client.cgi_to_server_fd);
								fd_to_pfd_index[pfds.back().fd] = cgi_to_server_pfd_index;
								pfds[cgi_to_server_pfd_index] = pfds.back();
								pfds.pop_back();

								fd_to_pfd_index.erase(client.cgi_to_server_fd);
								fd_to_client_index.erase(client.cgi_to_server_fd);

								if (close(client.cgi_to_server_fd) == -1)
								{
									perror("close");
									exit(EXIT_FAILURE);
								}
								client.cgi_to_server_fd = -1;
							}

							continue;
						}
					}
					else
					{
						// TODO: ??
						assert(false);
					}
				}

				// If we can read
				if (pfds[pfd_index].revents & POLLIN_ANY)
				{
					if (fd_type == FdType::SERVER)
					{
						int client_fd = accept(server_fd, NULL, NULL);
						std::cerr << "    Accepted client fd " << client_fd << std::endl;

						fd_to_pfd_index.insert(std::make_pair(client_fd, pfds.size()));

						fd_to_client_index.insert(std::make_pair(client_fd, clients.size()));

						pollfd client_pfd;
						client_pfd.fd = client_fd;
						client_pfd.events = POLLIN;
						pfds.push_back(client_pfd);

						clients.push_back(Client(client_fd));

						fd_to_fd_type.insert(std::make_pair(client_fd, FdType::CLIENT));
					}
					else
					{
						assert(fd_type == FdType::CLIENT || fd_type == FdType::CGI_TO_SERVER);

						Client &client = getClient(fd_to_client_index, fd, clients);

						ClientReadState::ClientReadState previous_read_state = client.client_read_state;

						if (!client.readFd(pfds, fd_to_pfd_index, fd_type))
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

								std::cerr << "    Child is going to exec Python" << std::endl;
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
							// std::cerr << "    Server closed server_to_cgi_tube[PIPE_READ_INDEX] fd " << server_to_cgi_tube[PIPE_READ_INDEX] << std::endl;
							close(cgi_to_server_tube[PIPE_WRITE_INDEX]);
							// std::cerr << "    Server closed cgi_to_server_tube[PIPE_WRITE_INDEX] fd " << cgi_to_server_tube[PIPE_WRITE_INDEX] << std::endl;

							int server_to_cgi_fd = server_to_cgi_tube[PIPE_WRITE_INDEX];

							fd_to_pfd_index.insert(std::make_pair(server_to_cgi_fd, pfds.size()));
							pollfd server_to_cgi_pfd;
							server_to_cgi_pfd.fd = server_to_cgi_fd;
							server_to_cgi_pfd.events = client.body.empty() ? 0 : POLLOUT;
							// std::cerr << "    have_read_body is " << client.have_read_body << std::endl;
							pfds.push_back(server_to_cgi_pfd);

							client.server_to_cgi_fd = server_to_cgi_fd;

							size_t client_index = fd_to_client_index.at(fd);
							fd_to_client_index.insert(std::make_pair(server_to_cgi_fd, client_index));
							client.cgi_write_state = CGIWriteState::WRITING_TO_CGI;
							fd_to_fd_type.insert(std::make_pair(server_to_cgi_fd, FdType::SERVER_TO_CGI));

							std::cerr << "    Added server_to_cgi fd " << server_to_cgi_fd << std::endl;

							int cgi_to_server_fd = cgi_to_server_tube[PIPE_READ_INDEX];

							fd_to_pfd_index.insert(std::make_pair(cgi_to_server_fd, pfds.size()));
							pollfd cgi_to_server_pfd;
							cgi_to_server_pfd.fd = cgi_to_server_fd;
							cgi_to_server_pfd.events = POLLIN;
							pfds.push_back(cgi_to_server_pfd);

							client.cgi_to_server_fd = cgi_to_server_fd;

							fd_to_client_index.insert(std::make_pair(cgi_to_server_fd, client_index));
							client.cgi_read_state = CGIReadState::READING_FROM_CGI;
							fd_to_fd_type.insert(std::make_pair(cgi_to_server_fd, FdType::CGI_TO_SERVER));

							std::cerr << "    Added cgi_to_server fd " << cgi_to_server_fd << std::endl;
						}
					}
				}

				// If we can write
				if (pfds[pfd_index].revents & POLLOUT_ANY)
				{
					Client &client = getClient(fd_to_client_index, fd, clients);

					if (fd_type == FdType::SERVER_TO_CGI)
					{
						// TODO: Remove this before the evaluation
						assert(client.cgi_write_state == CGIWriteState::WRITING_TO_CGI);

						size_t max_cgi_write_len = MAX_CGI_WRITE_LEN; // TODO: Read from config
						size_t body_substr_len = std::min(client.body.length() - client.body_index, max_cgi_write_len);

						// TODO: Remove this before the evaluation
						assert(body_substr_len > 0);

						// TODO: substr() can fail
						std::string body_substr = client.body.substr(client.body_index, body_substr_len);

						client.body_index += body_substr_len;

						std::cerr << "    Sending this body substr to the CGI that has a len of " << body_substr.length() << " bytes:\n----------\n" << body_substr << "\n----------" << std::endl;

						// TODO: Don't ignore errors
						write(client.server_to_cgi_fd, body_substr.c_str(), body_substr.length());

						// If we don't have anything left to write at the moment
						if (client.body_index == client.body.length())
						{
							std::cerr << "    Disabling POLLOUT" << std::endl;
							pfds[pfd_index].events &= ~POLLOUT;

							if (client.client_read_state == ClientReadState::DONE)
							{
								std::cerr << "    Closing server_to_cgi fd " << client.server_to_cgi_fd << std::endl;

								client.cgi_write_state = CGIWriteState::DONE;

								// Swap-remove
								size_t server_to_cgi_pfd_index = fd_to_pfd_index.at(client.server_to_cgi_fd);
								fd_to_pfd_index[pfds.back().fd] = server_to_cgi_pfd_index;
								pfds[server_to_cgi_pfd_index] = pfds.back();
								pfds.pop_back();

								fd_to_pfd_index.erase(client.server_to_cgi_fd);
								fd_to_client_index.erase(client.server_to_cgi_fd);

								if (close(client.server_to_cgi_fd) == -1)
								{
									perror("close");
									exit(EXIT_FAILURE);
								}
								client.server_to_cgi_fd = -1;
							}
						}
					}
					else if (fd_type == FdType::CLIENT)
					{
						// TODO: Remove this before the evaluation
						assert(client.client_write_state == ClientWriteState::WRITING_TO_CLIENT);

						size_t max_client_write_len = MAX_CLIENT_WRITE_LEN; // TODO: Read from config
						size_t response_substr_len = std::min(client.response.length() - client.response_index, max_client_write_len);

						// TODO: Remove this before the evaluation
						assert(response_substr_len > 0);

						// TODO: substr() can fail
						std::string response_substr = client.response.substr(client.response_index, response_substr_len);

						client.response_index += response_substr_len;

						std::cerr << "    Sending this response substr that has a len of " << response_substr.length() << " bytes:\n----------\n" << response_substr << "\n----------" << std::endl;

						// TODO: Don't ignore errors
						write(fd, response_substr.c_str(), response_substr.length());


						// TODO: Close
						// std::cerr << "    Closing client fd " << fd << std::endl;
						// TODO: Don't *always* close right after a single write


						// fd_to_pfd_index.erase(fd);
						// fd_to_client_index.erase(fd);

						// if (close(fd) == -1)
						// {
						// 	perror("close");
						// 	exit(EXIT_FAILURE);
						// }
						// client.client_fd = -1;

						// fd_to_pfd_index[pfds.back().fd] = pfd_index;

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
							std::cerr << "    Disabling POLLOUT" << std::endl;
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
			}
		}
	}

	return EXIT_SUCCESS;
}
