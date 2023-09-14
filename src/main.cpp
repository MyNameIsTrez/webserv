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
	std::unordered_map<pid_t, int> cgi_pid_to_client_fd;
	std::unordered_map<int, size_t> fd_to_client_index;
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
	using namespace Signal;

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
			assert(false);
		}

		int client_fd = cgi_pid_to_client_fd.at(child_pid);
		size_t client_index = fd_to_client_index.at(client_fd);
		Client &client = clients.at(client_index);

		assert(client.client_read_state == ClientReadState::DONE);
		assert(client.cgi_write_state == CGIWriteState::DONE);
		assert(client.cgi_read_state == CGIReadState::DONE);
		assert(client.client_write_state != ClientWriteState::DONE);

		client.cgi_exit_status = child_exit_status;

		size_t client_pfd_index = fd_to_pfd_index.at(client.client_fd);
		std::cerr << "    Enabling client POLLOUT" << std::endl;
		pfds[client_pfd_index].events |= POLLOUT;

		client.client_write_state = ClientWriteState::WRITING_TO_CLIENT;
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
	using namespace Signal;

	shutting_down_gracefully = true;
}

// vector=[ A, B, C, D ] with index=1 results in [ A, D, C ]
template <typename T>
static void swapRemove(T &vector, size_t index)
{
	vector[index] = vector.back();
	vector.pop_back();
}

static void removeClient(int fd, nfds_t pfd_index)
{
	using namespace Signal;

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
		size_t server_to_cgi_pfd_index = fd_to_pfd_index.at(client.server_to_cgi_fd);
		fd_to_pfd_index[pfds.back().fd] = server_to_cgi_pfd_index;
		swapRemove(pfds, server_to_cgi_pfd_index);

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
		size_t cgi_to_server_pfd_index = fd_to_pfd_index.at(client.cgi_to_server_fd);
		fd_to_pfd_index[pfds.back().fd] = cgi_to_server_pfd_index;
		swapRemove(pfds, cgi_to_server_pfd_index);

		fd_to_pfd_index.erase(client.cgi_to_server_fd);
		fd_to_client_index.erase(client.cgi_to_server_fd);

		if (close(client.cgi_to_server_fd) == -1)
		{
			perror("close");
			exit(EXIT_FAILURE);
		}
		client.cgi_to_server_fd = -1;
	}

	if (client.cgi_pid != -1)
	{
		cgi_pid_to_client_fd.erase(client.cgi_pid);
		client.cgi_pid = -1;
		// TODO: Should we kill()/signal() the child CGI process to prevent them being zombies?
	}

	fd_to_pfd_index.erase(client.client_fd);
	fd_to_pfd_index[pfds.back().fd] = pfd_index;
	swapRemove(pfds, pfd_index);

	// TODO: Is it possible for client.client_fd to have already been closed and erased?
	size_t client_index = fd_to_client_index.at(client.client_fd);
	fd_to_client_index.erase(client.client_fd);
	fd_to_client_index[clients.back().client_fd] = client_index;
	swapRemove(clients, client_index);
}

static void printEvents(const pollfd &pfd)
{
	std::cerr
		<< "  fd=" << pfd.fd << "; "
		<< "Events: "
		<< ((pfd.revents & POLLIN) ? "POLLIN " : "")
		<< ((pfd.revents & POLLOUT) ? "POLLOUT " : "")
		<< ((pfd.revents & POLLHUP) ? "POLLHUP " : "")
		<< ((pfd.revents & POLLNVAL) ? "POLLNVAL " : "")
		<< ((pfd.revents & POLLPRI) ? "POLLPRI " : "")
		<< ((pfd.revents & POLLRDBAND) ? "POLLRDBAND " : "")
		<< ((pfd.revents & POLLRDNORM) ? "POLLRDNORM " : "")
		<< ((pfd.revents & POLLWRBAND) ? "POLLWRBAND " : "")
		<< ((pfd.revents & POLLWRNORM) ? "POLLWRNORM " : "")
		<< ((pfd.revents & POLLERR) ? "POLLERR " : "")
		<< std::endl;
}

static void pollhupServerToCGI()
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
}

static void pollhupCGIToServer(int fd)
{
	using namespace Signal;

	Client &client = getClient(fd_to_client_index, fd, clients);

	// TODO: .erase(client.cgi_pid), and possibly also kill()/signal() it here?

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

		std::cerr << "    Removing server_to_cgi from pfds" << std::endl;
		size_t server_to_cgi_pfd_index = fd_to_pfd_index.at(client.server_to_cgi_fd);
		fd_to_pfd_index[pfds.back().fd] = server_to_cgi_pfd_index;
		swapRemove(pfds, server_to_cgi_pfd_index);

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

		std::cerr << "    Removing cgi_to_server from pfds" << std::endl;
		size_t cgi_to_server_pfd_index = fd_to_pfd_index.at(client.cgi_to_server_fd);
		fd_to_pfd_index[pfds.back().fd] = cgi_to_server_pfd_index;
		swapRemove(pfds, cgi_to_server_pfd_index);

		fd_to_pfd_index.erase(client.cgi_to_server_fd);
		fd_to_client_index.erase(client.cgi_to_server_fd);

		if (close(client.cgi_to_server_fd) == -1)
		{
			perror("close");
			exit(EXIT_FAILURE);
		}
		client.cgi_to_server_fd = -1;
	}
}

static void acceptClient(int server_fd, std::unordered_map<int, FdType::FdType> &fd_to_fd_type)
{
	using namespace Signal;

	int client_fd = accept(server_fd, NULL, NULL);
	std::cerr << "    Accepted client fd " << client_fd << std::endl;

	// TODO: Handle accept() failing. Specifically handle too many open fds gracefully

	fd_to_pfd_index.emplace(client_fd, pfds.size());

	fd_to_client_index.emplace(client_fd, clients.size());

	pollfd client_pfd;
	client_pfd.fd = client_fd;
	client_pfd.events = POLLIN;
	pfds.push_back(client_pfd);

	clients.push_back(Client(client_fd));

	fd_to_fd_type.emplace(client_fd, FdType::CLIENT);
}

static bool startCGI(FdType::FdType fd_type, Client &client, int fd, std::unordered_map<int, FdType::FdType> &fd_to_fd_type)
{
	using namespace Signal;

	assert(fd_type == FdType::CLIENT);

	int server_to_cgi_tube[2];
	int cgi_to_server_tube[2];

	if (pipe(server_to_cgi_tube) == -1)
	{
		perror("pipe");
		return false;
	}
	if (pipe(cgi_to_server_tube) == -1)
	{
		perror("pipe");
		return false;
	}

	pid_t forked_pid = fork();
	if (forked_pid == -1)
	{
		perror("fork");
		return false;
	}
	else if (forked_pid == CHILD)
	{
		signal(SIGINT, SIG_IGN);

		close(server_to_cgi_tube[PIPE_WRITE_INDEX]);
		close(cgi_to_server_tube[PIPE_READ_INDEX]);

		if (dup2(server_to_cgi_tube[PIPE_READ_INDEX], STDIN_FILENO) == -1)
		{
			perror("dup2");
			return false;
		}
		close(server_to_cgi_tube[PIPE_READ_INDEX]);

		if (dup2(cgi_to_server_tube[PIPE_WRITE_INDEX], STDOUT_FILENO) == -1)
		{
			perror("dup2");
			return false;
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
		return false;
	}

	close(server_to_cgi_tube[PIPE_READ_INDEX]);
	// std::cerr << "    Server closed server_to_cgi_tube[PIPE_READ_INDEX] fd " << server_to_cgi_tube[PIPE_READ_INDEX] << std::endl;
	close(cgi_to_server_tube[PIPE_WRITE_INDEX]);
	// std::cerr << "    Server closed cgi_to_server_tube[PIPE_WRITE_INDEX] fd " << cgi_to_server_tube[PIPE_WRITE_INDEX] << std::endl;

	client.cgi_pid = forked_pid;

	cgi_pid_to_client_fd.emplace(forked_pid, fd);

	size_t client_index = fd_to_client_index.at(fd);

	int server_to_cgi_fd = server_to_cgi_tube[PIPE_WRITE_INDEX];

	fd_to_pfd_index.emplace(server_to_cgi_fd, pfds.size());
	pollfd server_to_cgi_pfd;
	server_to_cgi_pfd.fd = server_to_cgi_fd;
	server_to_cgi_pfd.events = client.body.empty() ? 0 : POLLOUT;
	// std::cerr << "    have_read_body is " << client.have_read_body << std::endl;
	pfds.push_back(server_to_cgi_pfd);

	client.server_to_cgi_fd = server_to_cgi_fd;

	fd_to_client_index.emplace(server_to_cgi_fd, client_index);
	client.cgi_write_state = CGIWriteState::WRITING_TO_CGI;
	fd_to_fd_type.emplace(server_to_cgi_fd, FdType::SERVER_TO_CGI);

	std::cerr << "    Added server_to_cgi fd " << server_to_cgi_fd << std::endl;

	int cgi_to_server_fd = cgi_to_server_tube[PIPE_READ_INDEX];

	fd_to_pfd_index.emplace(cgi_to_server_fd, pfds.size());
	pollfd cgi_to_server_pfd;
	cgi_to_server_pfd.fd = cgi_to_server_fd;
	cgi_to_server_pfd.events = POLLIN;
	pfds.push_back(cgi_to_server_pfd);

	client.cgi_to_server_fd = cgi_to_server_fd;

	fd_to_client_index.emplace(cgi_to_server_fd, client_index);
	client.cgi_read_state = CGIReadState::READING_FROM_CGI;
	fd_to_fd_type.emplace(cgi_to_server_fd, FdType::CGI_TO_SERVER);

	std::cerr << "    Added cgi_to_server fd " << cgi_to_server_fd << std::endl;

	return true;
}

static void writeServerToCGI(Client &client, nfds_t pfd_index)
{
	using namespace Signal;

	assert(client.cgi_write_state == CGIWriteState::WRITING_TO_CGI);

	size_t max_cgi_write_len = MAX_CGI_WRITE_LEN; // TODO: Read from config
	size_t body_substr_len = std::min(client.body.length() - client.body_index, max_cgi_write_len);

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

			size_t server_to_cgi_pfd_index = fd_to_pfd_index.at(client.server_to_cgi_fd);
			fd_to_pfd_index[pfds.back().fd] = server_to_cgi_pfd_index;
			swapRemove(pfds, server_to_cgi_pfd_index);

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

static void writeToClient(Client &client, int fd, nfds_t pfd_index)
{
	using namespace Signal;

	assert(client.client_write_state == ClientWriteState::WRITING_TO_CLIENT);

	size_t max_client_write_len = MAX_CLIENT_WRITE_LEN; // TODO: Read from config
	size_t response_substr_len = std::min(client.response.length() - client.response_index, max_client_write_len);

	assert(response_substr_len > 0);

	// TODO: substr() can fail
	std::string response_substr = client.response.substr(client.response_index, response_substr_len);

	client.response_index += response_substr_len;

	std::cerr << "    Sending this response substr that has a len of " << response_substr.length() << " bytes:\n----------\n" << response_substr << "\n----------" << std::endl;

	// TODO: Don't ignore errors
	write(fd, response_substr.c_str(), response_substr.length());

	// TODO: Close the client at some point
	// std::cerr << "    Closing client fd " << fd << std::endl;

	// If we don't have anything left to write at the moment
	if (client.response_index == client.response.length())
	{
		std::cerr << "    Disabling POLLOUT" << std::endl;
		pfds[pfd_index].events &= ~POLLOUT;
	}
}

// c++ main.cpp Client.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
// Code stolen from https://youtu.be/esXw4bdaZkc
int main(void)
{
	using namespace Signal;
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

	// fd_to_pfd_index.emplace(server_fd, pfds.size());

	pollfd server_pfd;
	server_pfd.fd = server_fd;
	server_pfd.events = POLLIN;
	pfds.push_back(server_pfd);

	std::cerr << "Port is " << SERVER_PORT << std::endl;

	std::unordered_map<int, FdType::FdType> fd_to_fd_type;

	fd_to_fd_type.emplace(server_fd, FdType::SERVER);

	signal(SIGCHLD, sigChildHandler);
	signal(SIGINT, sigIntHandler);

	bool servers_active = true;
	while (true)
	{
		if (shutting_down_gracefully && servers_active)
		{
			std::cerr << "Shutting down gracefully..." << std::endl;

			// TODO: Handle multiple servers; the required steps are listed here: https://stackoverflow.com/a/15560580/13279557
			size_t server_pfd_index = 0;
			fd_to_pfd_index[pfds.back().fd] = server_pfd_index;
			swapRemove(pfds, server_pfd_index);

			servers_active = false;
		}

		if (pfds.empty())
		{
			break;
		}
		else if (shutting_down_gracefully) {
			std::cerr << "Waiting on " << pfds.size() << " fds..." << std::endl;

			// TODO: Do we want to use : iteration in other spots too?
			for (pollfd pfd : pfds) {
				std::cerr << "	Waiting on poll fd " << pfd.fd << std::endl;
			}
		}

		std::cerr << "Waiting on an event..." << std::endl;
		int event_count = poll(pfds.data(), pfds.size(), -1);
		if (event_count == -1)
		{
			if (errno == EINTR)
			{
				continue;
			}
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

				printEvents(pfds[pfd_index]);

				// This can be reached by commenting out a line that removes a closed fd from pfds
				if (pfds[pfd_index].revents & POLLNVAL)
				{
					// TODO: Should the server exit, or should the client be removed?
					assert(false);
				}

				// If there was an error, remove the client, and close all its file descriptors
				if (pfds[pfd_index].revents & POLLERR)
				{
					removeClient(fd, pfd_index);
					continue;
				}

				// If the other end hung up (closed)
				if (pfds[pfd_index].revents & POLLHUP)
				{
					// If the Python script closed its stdin
					if (fd_type == FdType::SERVER_TO_CGI)
					{
						pollhupServerToCGI();
						continue;
					}
					// If the Python script closed its stdout
					else if (fd_type == FdType::CGI_TO_SERVER)
					{
						// If the server has read everything from the Python script
						if (!(pfds[pfd_index].revents & POLLIN))
						{
							pollhupCGIToServer(fd);
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
						acceptClient(server_fd, fd_to_fd_type);
					}
					else
					{
						assert(fd_type == FdType::CLIENT || fd_type == FdType::CGI_TO_SERVER);

						Client &client = getClient(fd_to_client_index, fd, clients);

						ClientReadState::ClientReadState previous_read_state = client.client_read_state;

						if (!client.readFd(pfds, fd_to_pfd_index, fd_type))
						{
							// TODO: Print error
							return EXIT_FAILURE;
						}

						// If we've just started reading this client's body
						if (previous_read_state != ClientReadState::BODY && client.client_read_state == ClientReadState::BODY)
						{
							// TODO: Only run the below code if the request wants to start the CGI

							if (!startCGI(fd_type, client, fd, fd_to_fd_type))
							{
								return EXIT_FAILURE;
							}
						}
					}
				}

				// If we can write
				if (pfds[pfd_index].revents & POLLOUT_ANY)
				{
					Client &client = getClient(fd_to_client_index, fd, clients);

					if (fd_type == FdType::SERVER_TO_CGI)
					{
						writeServerToCGI(client, pfd_index);
					}
					else if (fd_type == FdType::CLIENT)
					{
						writeToClient(client, fd, pfd_index);
					}
					else
					{
						// TODO: Should be unreachable
						assert(false);
					}
				}
			}
		}
	}

	return EXIT_SUCCESS;
}
