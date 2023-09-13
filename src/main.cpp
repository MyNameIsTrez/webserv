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

namespace Signal
{
	bool shutting_down_gracefully = false;
	std::unordered_map<pid_t, size_t> child_pid_to_client_index;
	std::vector<Client> clients;
	std::vector<pollfd> pfds;
	std::unordered_map<int, size_t> fd_to_pfd_index;
}

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

		size_t client_index = Signal::child_pid_to_client_index.at(child_pid);
		Client &client = Signal::clients.at(client_index);

		assert(client.client_read_state == ClientReadState::DONE);
		assert(client.cgi_write_state == CGIWriteState::DONE);
		assert(client.cgi_read_state == CGIReadState::DONE);
		assert(client.client_write_state != ClientWriteState::DONE);

		client.cgi_exit_status = child_exit_status;

		size_t client_pfd_index = Signal::fd_to_pfd_index.at(client.client_fd);
		std::cerr << "    Enabling client POLLOUT" << std::endl;
		Signal::pfds[client_pfd_index].events |= POLLOUT;
	}
	// TODO: Decide what to do when errno is EINTR
	// errno is set to ECHILD when there are no children left to wait for
	if (child_pid == -1 && errno != ECHILD)
	{
		perror("waitpid");
		exit(EXIT_FAILURE);
	}
}

static void sigIntHandler(int signum)
{
	(void)signum;
	Signal::shutting_down_gracefully = true;
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

	// fd_to_pfd_index.insert(std::make_pair(server_fd, pfds.size()));

	pollfd server_pfd;
	server_pfd.fd = server_fd;
	server_pfd.events = POLLIN;
	Signal::pfds.push_back(server_pfd);

	std::cerr << "Port is " << SERVER_PORT << std::endl;

	std::unordered_map<int, size_t> fd_to_client_index;

	std::unordered_map<int, FdType::FdType> fd_to_fd_type;

	fd_to_fd_type.insert(std::make_pair(server_fd, FdType::SERVER));

	signal(SIGCHLD, sigChildHandler);
	signal(SIGINT, sigIntHandler);

	bool servers_active = true;
	while (true)
	{
		if (Signal::shutting_down_gracefully && servers_active)
		{
			std::cerr << "Shutting down gracefully..." << std::endl;
			// TODO: Handle multiple servers. Can bind() let an fd point to multiple sockaddr_in structs?
			Signal::fd_to_pfd_index[Signal::pfds.back().fd] = 0;
			Signal::pfds[0] = Signal::pfds.back();
			Signal::pfds.pop_back();
			servers_active = false;
		}

		if (Signal::pfds.empty())
		{
			break;
		}
		else if (Signal::shutting_down_gracefully) {
			std::cerr << "Waiting on " << Signal::pfds.size() << " fds..." << std::endl;

			// TODO: Do we want to use : iteration in other spots too?
			for (pollfd pfd : Signal::pfds) {
				std::cerr << "	Waiting on poll fd " << pfd.fd << std::endl;
			}
		}

		std::cerr << "Waiting on an event..." << std::endl;
		int event_count = poll(Signal::pfds.data(), Signal::pfds.size(), -1);
		if (event_count == -1)
		{
			if (errno == EINTR)
			{
				continue;
			}
			perror("poll");
			exit(EXIT_FAILURE);
		}

		for (nfds_t pfd_index = Signal::pfds.size(); pfd_index > 0;)
		{
			pfd_index--;

			if (Signal::pfds[pfd_index].revents != 0)
			{
				int fd = Signal::pfds[pfd_index].fd;
				FdType::FdType fd_type = fd_to_fd_type.at(fd);

				std::cerr << "  fd=" << Signal::pfds[pfd_index].fd << "; "
						  << "Events: "
						  << ((Signal::pfds[pfd_index].revents & POLLIN) ? "POLLIN " : "")
						  << ((Signal::pfds[pfd_index].revents & POLLOUT) ? "POLLOUT " : "")
						  << ((Signal::pfds[pfd_index].revents & POLLHUP) ? "POLLHUP " : "")
						  << ((Signal::pfds[pfd_index].revents & POLLNVAL) ? "POLLNVAL " : "")
						  << ((Signal::pfds[pfd_index].revents & POLLPRI) ? "POLLPRI " : "")
						  << ((Signal::pfds[pfd_index].revents & POLLRDBAND) ? "POLLRDBAND " : "")
						  << ((Signal::pfds[pfd_index].revents & POLLRDNORM) ? "POLLRDNORM " : "")
						  << ((Signal::pfds[pfd_index].revents & POLLWRBAND) ? "POLLWRBAND " : "")
						  << ((Signal::pfds[pfd_index].revents & POLLWRNORM) ? "POLLWRNORM " : "")
						  << ((Signal::pfds[pfd_index].revents & POLLERR) ? "POLLERR " : "")
						  << std::endl;

				// This can be reached by commenting out a line that removes a closed fd from pfds
				if (Signal::pfds[pfd_index].revents & POLLNVAL)
				{
					// Should be unreachable
					// TODO: Remove this before the evaluation
					assert(false);
				}

				// If there was an error, remove the client, and close all its file descriptors
				if (Signal::pfds[pfd_index].revents & POLLERR)
				{
					Client &client = getClient(fd_to_client_index, fd, Signal::clients);

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
						size_t server_to_cgi_pfd_index = Signal::fd_to_pfd_index.at(client.server_to_cgi_fd);
						Signal::fd_to_pfd_index[Signal::pfds.back().fd] = server_to_cgi_pfd_index;
						Signal::pfds[server_to_cgi_pfd_index] = Signal::pfds.back();
						Signal::pfds.pop_back();

						Signal::fd_to_pfd_index.erase(client.server_to_cgi_fd);
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
						size_t cgi_to_server_pfd_index = Signal::fd_to_pfd_index.at(client.cgi_to_server_fd);
						Signal::fd_to_pfd_index[Signal::pfds.back().fd] = cgi_to_server_pfd_index;
						Signal::pfds[cgi_to_server_pfd_index] = Signal::pfds.back();
						Signal::pfds.pop_back();

						Signal::fd_to_pfd_index.erase(client.cgi_to_server_fd);
						fd_to_client_index.erase(client.cgi_to_server_fd);

						if (close(client.cgi_to_server_fd) == -1)
						{
							perror("close");
							exit(EXIT_FAILURE);
						}
						client.cgi_to_server_fd = -1;
					}

					// Swap-remove
					Signal::fd_to_pfd_index.erase(client.client_fd);
					Signal::fd_to_pfd_index[Signal::pfds.back().fd] = pfd_index;
					// Explanation: If pfds.size() == 2, with pfd[1] firing POLLIN and pfd[0] firing POLLHUP,
					// pfd[1] will be handled first, and then pfd[0] will run the below two lines, moving pfd[1] to pfds[0].
					// This is fine however, since the for-loop's pfd_index > 0 condition will be false, meaning pfd[1] won't be processed twice.
					//
					// In this visualization, the processed pfd is put in <>:
					// [ POLLHUP_A, POLLHUP_B, <POLLIN> ] -> [ POLLHUP_A, <POLLHUP_B>, POLLIN ] -> [ <POLLHUP_A>, POLLIN ] -> [ POLLIN ]
					Signal::pfds[pfd_index] = Signal::pfds.back();
					Signal::pfds.pop_back();

					// Swap-remove
					// TODO: Is it possible for client.client_fd to have already been closed and erased?
					size_t client_index = fd_to_client_index.at(client.client_fd);
					fd_to_client_index.erase(client.client_fd);
					fd_to_client_index[Signal::clients.back().client_fd] = client_index;
					Signal::clients[client_index] = Signal::clients.back();
					Signal::clients.pop_back();

					continue;
				}

				// If the other end hung up (closed)
				if (Signal::pfds[pfd_index].revents & POLLHUP)
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
						if (!(Signal::pfds[pfd_index].revents & POLLIN))
						{
							Client &client = getClient(fd_to_client_index, fd, Signal::clients);

							// Disable client POLLIN
							// We don't care that we could've read some more of the client's body
							if (client.client_read_state != ClientReadState::DONE)
							{
								client.client_read_state = ClientReadState::DONE;

								size_t client_pfd_index = Signal::fd_to_pfd_index.at(client.client_fd);
								std::cerr << "    Disabling client POLLIN" << std::endl;
								Signal::pfds[client_pfd_index].events &= ~POLLIN;
							}

							// Close and remove server_to_cgi
							// We don't care that we could've written some more of the client's body
							if (client.server_to_cgi_fd != -1)
							{
								client.cgi_write_state = CGIWriteState::DONE;

								// Swap-remove
								std::cerr << "    Removing server_to_cgi from pfds" << std::endl;
								size_t server_to_cgi_pfd_index = Signal::fd_to_pfd_index.at(client.server_to_cgi_fd);
								Signal::fd_to_pfd_index[Signal::pfds.back().fd] = server_to_cgi_pfd_index;
								Signal::pfds[server_to_cgi_pfd_index] = Signal::pfds.back();
								Signal::pfds.pop_back();

								Signal::fd_to_pfd_index.erase(client.server_to_cgi_fd);
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
								size_t cgi_to_server_pfd_index = Signal::fd_to_pfd_index.at(client.cgi_to_server_fd);
								Signal::fd_to_pfd_index[Signal::pfds.back().fd] = cgi_to_server_pfd_index;
								Signal::pfds[cgi_to_server_pfd_index] = Signal::pfds.back();
								Signal::pfds.pop_back();

								Signal::fd_to_pfd_index.erase(client.cgi_to_server_fd);
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
				if (Signal::pfds[pfd_index].revents & POLLIN_ANY)
				{
					if (fd_type == FdType::SERVER)
					{
						int client_fd = accept(server_fd, NULL, NULL);
						std::cerr << "    Accepted client fd " << client_fd << std::endl;

						// TODO: Handle accept() failing. Specifically handle too many open fds gracefully

						Signal::fd_to_pfd_index.insert(std::make_pair(client_fd, Signal::pfds.size()));

						fd_to_client_index.insert(std::make_pair(client_fd, Signal::clients.size()));

						pollfd client_pfd;
						client_pfd.fd = client_fd;
						client_pfd.events = POLLIN;
						Signal::pfds.push_back(client_pfd);

						Signal::clients.push_back(Client(client_fd));

						fd_to_fd_type.insert(std::make_pair(client_fd, FdType::CLIENT));
					}
					else
					{
						assert(fd_type == FdType::CLIENT || fd_type == FdType::CGI_TO_SERVER);

						Client &client = getClient(fd_to_client_index, fd, Signal::clients);

						ClientReadState::ClientReadState previous_read_state = client.client_read_state;

						if (!client.readFd(Signal::pfds, Signal::fd_to_pfd_index, fd_type))
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
								signal(SIGINT, SIG_IGN);

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

							size_t client_index = fd_to_client_index.at(fd);
							// TODO: Make sure that every swap-remove spot also updates this!
							Signal::child_pid_to_client_index.insert(std::make_pair(forked_pid, client_index));

							int server_to_cgi_fd = server_to_cgi_tube[PIPE_WRITE_INDEX];

							Signal::fd_to_pfd_index.insert(std::make_pair(server_to_cgi_fd, Signal::pfds.size()));
							pollfd server_to_cgi_pfd;
							server_to_cgi_pfd.fd = server_to_cgi_fd;
							server_to_cgi_pfd.events = client.body.empty() ? 0 : POLLOUT;
							// std::cerr << "    have_read_body is " << client.have_read_body << std::endl;
							Signal::pfds.push_back(server_to_cgi_pfd);

							client.server_to_cgi_fd = server_to_cgi_fd;

							fd_to_client_index.insert(std::make_pair(server_to_cgi_fd, client_index));
							client.cgi_write_state = CGIWriteState::WRITING_TO_CGI;
							fd_to_fd_type.insert(std::make_pair(server_to_cgi_fd, FdType::SERVER_TO_CGI));

							std::cerr << "    Added server_to_cgi fd " << server_to_cgi_fd << std::endl;

							int cgi_to_server_fd = cgi_to_server_tube[PIPE_READ_INDEX];

							Signal::fd_to_pfd_index.insert(std::make_pair(cgi_to_server_fd, Signal::pfds.size()));
							pollfd cgi_to_server_pfd;
							cgi_to_server_pfd.fd = cgi_to_server_fd;
							cgi_to_server_pfd.events = POLLIN;
							Signal::pfds.push_back(cgi_to_server_pfd);

							client.cgi_to_server_fd = cgi_to_server_fd;

							fd_to_client_index.insert(std::make_pair(cgi_to_server_fd, client_index));
							client.cgi_read_state = CGIReadState::READING_FROM_CGI;
							fd_to_fd_type.insert(std::make_pair(cgi_to_server_fd, FdType::CGI_TO_SERVER));

							std::cerr << "    Added cgi_to_server fd " << cgi_to_server_fd << std::endl;
						}
					}
				}

				// If we can write
				if (Signal::pfds[pfd_index].revents & POLLOUT_ANY)
				{
					Client &client = getClient(fd_to_client_index, fd, Signal::clients);

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
							Signal::pfds[pfd_index].events &= ~POLLOUT;

							if (client.client_read_state == ClientReadState::DONE)
							{
								std::cerr << "    Closing server_to_cgi fd " << client.server_to_cgi_fd << std::endl;

								client.cgi_write_state = CGIWriteState::DONE;

								// Swap-remove
								size_t server_to_cgi_pfd_index = Signal::fd_to_pfd_index.at(client.server_to_cgi_fd);
								Signal::fd_to_pfd_index[Signal::pfds.back().fd] = server_to_cgi_pfd_index;
								Signal::pfds[server_to_cgi_pfd_index] = Signal::pfds.back();
								Signal::pfds.pop_back();

								Signal::fd_to_pfd_index.erase(client.server_to_cgi_fd);
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
							Signal::pfds[pfd_index].events &= ~POLLOUT;
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
