#include "Server.hpp"

#include "Client.hpp"

#include <cassert>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>

// TODO: Move some/all of these defines to a config file
#define SERVER_PORT 18000
#define MAX_CONNECTION_QUEUE_LEN 10
#define MAX_CGI_WRITE_LEN 3
#define MAX_CLIENT_WRITE_LEN 100
#define CHILD 0
#define PIPE_READ_INDEX 0
#define PIPE_WRITE_INDEX 1
#define MAX_RECEIVED_LEN 2

// TODO: Turn these into static ints inside of a class?
const int POLLIN_ANY = POLLIN | POLLRDBAND | POLLRDNORM | POLLPRI;
const int POLLOUT_ANY = POLLOUT | POLLWRBAND | POLLWRNORM;

/*	Constructors */

bool shutting_down_gracefully = false;

static void sigIntHandler(int signum)
{
	(void)signum;
	shutting_down_gracefully = true;
}

Server::Server(void)
	: server_fd(-1),
	  cgi_pid_to_client_fd(),
	  fd_to_client_index(),
	  fd_to_pfd_index(),
	  fd_to_fd_type(),
	  clients(),
	  pfds()
{
	// TODO: Parse config

	// The protocol 0 lets socket() pick a protocol, based on the requested socket type (stream)
	// Source: https://pubs.opengroup.org/onlinepubs/009695399/functions/socket.html
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket");
		exit(EXIT_FAILURE);
	}

	// Prevents "bind: Address already in use" error after:
	// 1. Starting a CGI script, 2. Doing Ctrl+\ on the server, 3. Restarting the server
	int option = 1; // "the parameter should be non-zero to enable a boolean option"
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

	fd_to_fd_type.emplace(server_fd, FdType::SERVER);

	signal(SIGINT, sigIntHandler);
}

Server::Server(const std::string &configuration_path)
	: Server()
{
	// TODO: Use the configuration_path
	(void)configuration_path;
}

/*	Public member functions */

Server::~Server(void)
{
}

void Server::run(void)
{
	// TODO: Handle multiple servers
	bool servers_active = true;

	while (true)
	{
		if (shutting_down_gracefully && servers_active)
		{
			std::cerr << std::endl << "Shutting down gracefully..." << std::endl;

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
			// TODO: Do we want to use : iteration in other spots too?
			for (pollfd pfd : pfds)
			{
				std::cerr << "  Waiting on poll fd " << pfd.fd << std::endl;
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
					removeClient(fd);
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
					else if (fd_type == FdType::CGI_EXIT_DETECTOR)
					{
						// We use a pipe-generating-POLLHUP-based approach, rather than signal(SIGCHLD, handler),
						// as the signal approach is what we did first and required a mess of global state
						// Suggested here as "Approach 1": https://stackoverflow.com/a/8976461/13279557

						pid_t child_pid;
						int child_exit_status;

						// Reaps all children that have exited
						// waitpid() returns 0 if no more children can be reaped right now
						// WNOHANG guarantees that this call doesn't block
						// This is done in a loop, since signals aren't queued: https://stackoverflow.com/a/45809843/13279557
						// TODO: Are there other options we should use?
						while ((child_pid = waitpid(-1, &child_exit_status, WNOHANG)) > 0)
						{
							if (WIFEXITED(child_exit_status))
							{
								std::cerr << "    PID " << child_pid << " exit status: " << WEXITSTATUS(child_exit_status) << std::endl;
							}
							else if (WIFSTOPPED(child_exit_status))
							{
								std::cerr << "    PID " << child_pid << " was stopped by " << WSTOPSIG(child_exit_status) << std::endl;
							}
							else if (WIFSIGNALED(child_exit_status))
							{
								std::cerr << "    PID " << child_pid << " exited due to signal " << WTERMSIG(child_exit_status) << std::endl;
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

							size_t client_pfd_index = fd_to_pfd_index.at(client.client_fd);
							std::cerr << "    Enabling client POLLOUT" << std::endl;
							pfds[client_pfd_index].events |= POLLOUT;

							client.client_write_state = ClientWriteState::WRITING_TO_CLIENT;

							client.prependResponseHeader(child_exit_status);
						}

						// TODO: Decide what to do when errno is EINTR
						// errno is set to ECHILD when there are no children left to wait for
						if (child_pid == -1 && errno != ECHILD)
						{
							perror("waitpid");
							exit(EXIT_FAILURE);
						}

						pollhupCGIExitDetector(fd);

						// TODO: Do we need to manually call methods here that remove server_to_cgi and cgi_to_server?

						continue;
					}
					else
					{
						// TODO: Should be unreachable
						assert(false);
					}
				}

				// If we can read
				if (pfds[pfd_index].revents & POLLIN_ANY)
				{
					if (fd_type == FdType::SERVER)
					{
						acceptClient();
					}
					else
					{
						assert(fd_type == FdType::CLIENT || fd_type == FdType::CGI_TO_SERVER);

						Client &client = getClient(fd);

						// TODO: Consider replacing all "continue"s with this bool
						bool removed_client = false;

						if (!readFd(client, fd, fd_type, removed_client))
						{
							// TODO: Print error
							exit(EXIT_FAILURE);
						}

						if (removed_client)
						{
							continue;
						}
					}
				}

				// If we can write
				if (pfds[pfd_index].revents & POLLOUT_ANY)
				{
					Client &client = getClient(fd);

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
}

/*	Private member functions */

// TODO: Use this on every function that can fail
// static void die()
// {
// 	// TODO: Loop over all fds and close them

// 	// TODO: We are not allowed to use perror(), but we can use strerror()

// 	exit(EXIT_FAILURE);
// }

// vector=[ A, B, C, D ] with index=1 results in [ A, D, C ]
template <typename T>
void Server::swapRemove(T &vector, size_t index)
{
	vector[index] = vector.back();
	vector.pop_back();
}

void Server::printEvents(const pollfd &pfd)
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

void Server::removeClient(int fd)
{
	std::cerr << "  Removing client with fd " << fd << std::endl;

	Client &client = getClient(fd);

	if (client.server_to_cgi_fd != -1)
	{
		removeFd(client.server_to_cgi_fd);
	}

	if (client.cgi_to_server_fd != -1)
	{
		removeFd(client.cgi_to_server_fd);
	}

	if (client.cgi_exit_detector_fd != -1)
	{
		removeFd(client.cgi_exit_detector_fd);
	}

	if (client.cgi_pid != -1)
	{
		kill(client.cgi_pid, SIGTERM);

		cgi_pid_to_client_fd.erase(client.cgi_pid);
		client.cgi_pid = -1;
	}

	// TODO: Is it possible for client.client_fd to have already been closed and erased; check if it's -1?
	size_t client_index = fd_to_client_index.at(client.client_fd);
	fd_to_client_index[clients.back().client_fd] = client_index;
	swapRemove(clients, client_index);

	removeFd(client.client_fd);
}

// TODO: Don't let fd_to_client_index nor clients be passed in, by just getting it from the member variable;
// TODO: Do the same for other methods that are getting useless args!
// This is called in many spots
// due to the fact that client pointer can dangle
// whenever the clients vector resizes
Client &Server::getClient(int fd)
{
	size_t client_index = fd_to_client_index.at(fd);
	return clients[client_index];
}

void Server::removeFd(int &fd)
{
	std::cerr << "    Removing fd " << fd << std::endl;

	size_t pfd_index = fd_to_pfd_index.at(fd);
	fd_to_pfd_index[pfds.back().fd] = pfd_index;
	swapRemove(pfds, pfd_index);

	fd_to_pfd_index.erase(fd);
	fd_to_client_index.erase(fd);

	if (close(fd) == -1)
	{
		perror("close");
		exit(EXIT_FAILURE);
	}

	fd = -1;
}

void Server::pollhupServerToCGI()
{
	std::cerr << "  In pollhupServerToCGI()" << std::endl;

	// TODO: ??
	assert(false);

	// Client &client = getClient(fd);

	// client.client_read_state = ClientReadState::DONE;

	// size_t client_pfd_index = fd_to_pfd_index.at(client.client_fd);
	// std::cerr << "    Disabling client POLLIN" << std::endl;
	// pfds[client_pfd_index].events &= ~POLLIN;

	// client.cgi_write_state = CGIWriteState::DONE;
	// size_t server_to_cgi_pfd_index = fd_to_pfd_index.at(client.server_to_cgi_fd);
	// std::cerr << "    Disabling server_to_cgi POLLOUT" << std::endl;
	// pfds[server_to_cgi_pfd_index].events &= ~POLLOUT;
}

void Server::pollhupCGIToServer(int fd)
{
	std::cerr << "  In pollhupCGIToServer()" << std::endl;

	Client &client = getClient(fd);

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
		removeServerToCGI(client);
	}

	// Close and remove cgi_to_server
	{
		assert(client.cgi_to_server_fd != -1);
		client.cgi_read_state = CGIReadState::DONE;

		removeFd(client.cgi_to_server_fd);
		client.cgi_to_server_fd = -1;
	}
}

void Server::removeServerToCGI(Client &client)
{
	client.cgi_write_state = CGIWriteState::DONE;

	removeFd(client.server_to_cgi_fd);
}

void Server::pollhupCGIExitDetector(int fd)
{
	std::cerr << "  In pollhupCGIExitDetector()" << std::endl;

	Client &client = getClient(fd);

	assert(client.cgi_exit_detector_fd != -1);

	removeFd(client.cgi_exit_detector_fd);
}

void Server::acceptClient()
{
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

bool Server::readFd(Client &client, int fd, FdType::FdType fd_type, bool &removed_client)
{
	char received[MAX_RECEIVED_LEN] = {};

	std::cerr << "    About to call read(" << fd << ", received, " << MAX_RECEIVED_LEN << ") on fd_type " << fd_type << std::endl;

	// TODO: We should never read past the content_length of the BODY
	ssize_t bytes_read = read(fd, received, MAX_RECEIVED_LEN);
	if (bytes_read == -1)
	{
		perror("read");
		exit(EXIT_FAILURE);
	}
	if (bytes_read == 0)
	{
		std::cerr << "    Read 0 bytes" << std::endl;

		// TODO: Assert that we reached content_length

		if (fd_type == FdType::CLIENT)
		{
			// TODO: Probably need to send the client a response like "expected more body bytes" if it's less than content_length

			removeClient(client.client_fd);
			removed_client = true;
		}
		else if (fd_type == FdType::CGI_TO_SERVER)
		{
			client.cgi_read_state = CGIReadState::DONE;

			size_t cgi_to_server_pfds_index = fd_to_pfd_index.at(client.cgi_to_server_fd);
			std::cerr << "    Disabling cgi_to_server POLLIN" << std::endl;
			pfds[cgi_to_server_pfds_index].events &= ~POLLIN;

			// TODO: .erase(client.cgi_pid), and possibly also kill()/signal() it here?
		}

		return true;
	}

	assert(client.cgi_read_state != CGIReadState::DONE);
	assert(client.client_write_state != ClientWriteState::DONE);

	std::cerr << "    Read " << bytes_read << " bytes:\n----------\n" << std::string(received, bytes_read) << "\n----------\n" << std::endl;

	if (fd_type == FdType::CLIENT)
	{
		ClientReadState::ClientReadState previous_read_state = client.client_read_state;

		if (!client.appendReadString(received, bytes_read))
		{
			return false;
		}

		// If we've just started reading/entirely read this client's body, start a CGI script
		if (previous_read_state == ClientReadState::HEADER && client.client_read_state != ClientReadState::HEADER)
		{
			// TODO: Only run the below code if the request wants to start the CGI

			if (!startCGI(client, fd, fd_type))
			{
				return false;
			}
		}

		if (client.client_read_state != ClientReadState::HEADER && !client.body.empty())
		{
			size_t server_to_cgi_pfds_index = fd_to_pfd_index.at(client.server_to_cgi_fd);
			std::cerr << "    Enabling server_to_cgi POLLOUT" << std::endl;
			pfds[server_to_cgi_pfds_index].events |= POLLOUT;
		}
	}
	else if (fd_type == FdType::CGI_TO_SERVER)
	{
		if (client.cgi_read_state == CGIReadState::READING_FROM_CGI)
		{
			client.response += std::string(received, bytes_read);
		}
		else
		{
			// TODO: Should be unreachable
			assert(false);
		}
	}
	else
	{
		// TODO: Should be unreachable
		assert(false);
	}

	return true;
}

bool Server::startCGI(Client &client, int fd, FdType::FdType fd_type)
{
	std::cerr << "  Starting CGI..." << std::endl;

	assert(fd_type == FdType::CLIENT);

	int server_to_cgi_tube[2];
	int cgi_to_server_tube[2];
	int cgi_exit_detector_tube[2];

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
	if (pipe(cgi_exit_detector_tube) == -1)
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
		close(cgi_exit_detector_tube[PIPE_READ_INDEX]);

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
		char *const argv[] = {(char *)"python3", (char *)"cgi-bin/print.py", NULL};

		// TODO: Construct cgi_env using header_map
		char *cgi_env[] = {NULL};
		execve(path, argv, cgi_env);

		perror("execve");
		return false;
	}

	close(server_to_cgi_tube[PIPE_READ_INDEX]);
	close(cgi_to_server_tube[PIPE_WRITE_INDEX]);
	// Whether the parent closes its READ or WRITE end might seem like an arbitrary choice,
	// but it absolutely isn't, since we want to generate a POLLHUP, and not a POLLERR
	close(cgi_exit_detector_tube[PIPE_WRITE_INDEX]);

	client.cgi_pid = forked_pid;

	cgi_pid_to_client_fd.emplace(forked_pid, fd);

	size_t client_index = fd_to_client_index.at(fd);

	int server_to_cgi_fd = server_to_cgi_tube[PIPE_WRITE_INDEX];

	// TODO: If this is a GET or a POST (can they have a body?)
	if (client.client_read_state == ClientReadState::DONE && client.body.empty())
	{
		std::cerr << "    Closing server_to_cgi fd immediately, since there is no body" << std::endl;
		close(server_to_cgi_fd);
		client.cgi_write_state = CGIWriteState::DONE;
	}
	else
	{
		addCGIFd(server_to_cgi_fd, client_index, FdType::SERVER_TO_CGI, client.body.empty() ? 0 : POLLOUT);
		client.server_to_cgi_fd = server_to_cgi_fd;
		client.cgi_write_state = CGIWriteState::WRITING_TO_CGI;
		std::cerr << "    Added server_to_cgi fd " << server_to_cgi_fd << std::endl;
	}

	int cgi_to_server_fd = cgi_to_server_tube[PIPE_READ_INDEX];
	addCGIFd(cgi_to_server_fd, client_index, FdType::CGI_TO_SERVER, POLLIN);
	client.cgi_to_server_fd = cgi_to_server_fd;
	client.cgi_read_state = CGIReadState::READING_FROM_CGI;
	std::cerr << "    Added cgi_to_server fd " << cgi_to_server_fd << std::endl;

	int cgi_exit_detector_fd = cgi_exit_detector_tube[PIPE_READ_INDEX];
	addCGIFd(cgi_exit_detector_fd, client_index, FdType::CGI_EXIT_DETECTOR, 0);
	client.cgi_exit_detector_fd = cgi_exit_detector_fd;
	std::cerr << "    Added cgi_exit_detector fd " << cgi_exit_detector_fd << std::endl;

	return true;
}

void Server::addCGIFd(int cgi_fd, size_t client_index, FdType::FdType fd_type, short int events)
{
	fd_to_client_index.emplace(cgi_fd, client_index);
	fd_to_fd_type.emplace(cgi_fd, fd_type);
	fd_to_pfd_index.emplace(cgi_fd, pfds.size());

	pollfd server_to_cgi_pfd;
	server_to_cgi_pfd.fd = cgi_fd;
	server_to_cgi_pfd.events = events;
	pfds.push_back(server_to_cgi_pfd);
}

void Server::writeServerToCGI(Client &client, nfds_t pfd_index)
{
	std::cerr << "  Writing from the server to the CGI..." << std::endl;

	assert(client.cgi_write_state == CGIWriteState::WRITING_TO_CGI);

	size_t max_cgi_write_len = MAX_CGI_WRITE_LEN; // TODO: Read from config
	size_t body_substr_len = std::min(client.body.length() - client.body_index, max_cgi_write_len);

	assert(body_substr_len > 0);

	// TODO: substr() can fail
	std::string body_substr = client.body.substr(client.body_index, body_substr_len);

	client.body_index += body_substr_len;

	std::cerr << "    Sending this body substr to the CGI that has a length of " << body_substr.length() << " bytes:\n----------\n" << body_substr << "\n----------\n" << std::endl;

	// TODO: Don't ignore errors
	write(client.server_to_cgi_fd, body_substr.c_str(), body_substr.length());

	// If we don't have anything left to write at the moment
	if (client.body_index == client.body.length())
	{
		std::cerr << "    Disabling server_to_cgi POLLOUT" << std::endl;
		pfds[pfd_index].events &= ~POLLOUT;

		if (client.client_read_state == ClientReadState::DONE)
		{
			removeServerToCGI(client);
		}
	}
}

void Server::writeToClient(Client &client, int fd, nfds_t pfd_index)
{
	std::cerr << "  Writing to the client..." << std::endl;

	assert(client.client_write_state == ClientWriteState::WRITING_TO_CLIENT);

	size_t max_client_write_len = MAX_CLIENT_WRITE_LEN; // TODO: Read from config
	size_t response_substr_len = std::min(client.response.length() - client.response_index, max_client_write_len);

	assert(response_substr_len > 0);

	// TODO: substr() can fail
	std::string response_substr = client.response.substr(client.response_index, response_substr_len);

	client.response_index += response_substr_len;

	std::cerr << "    Sending this response substr that has a length of " << response_substr.length() << " bytes:\n----------\n" << response_substr << "\n----------\n" << std::endl;

	// TODO: Don't ignore errors
	write(fd, response_substr.c_str(), response_substr.length());

	// TODO: Close the client at some point
	// std::cerr << "    Closing client fd " << fd << std::endl;

	// If we don't have anything left to write at the moment
	if (client.response_index == client.response.length())
	{
		std::cerr << "    Disabling client POLLOUT" << std::endl;
		pfds[pfd_index].events &= ~POLLOUT;
	}
}
