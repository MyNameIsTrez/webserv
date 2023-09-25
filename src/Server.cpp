#include "Server.hpp"

#include "Client.hpp"

#include <cassert>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unordered_set>

// TODO: Move some/all of these defines to a config file
#define SERVER_PORT 18000
#define MAX_CONNECTION_QUEUE_LEN 10
#define MAX_CGI_WRITE_LEN 100
#define MAX_CLIENT_WRITE_LEN 100
#define CHILD 0
#define PIPE_READ_INDEX 0
#define PIPE_WRITE_INDEX 1
#define MAX_RECEIVED_LEN 100

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
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		exit(EXIT_FAILURE);
	}

	// Prevents "bind: Address already in use" error after:
	// 1. Starting a CGI script, 2. Doing Ctrl+\ on the server, 3. Restarting the server
	int option = 1; // "the parameter should be non-zero to enable a boolean option"
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1)
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	sockaddr_in servaddr{};
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERVER_PORT);

	if ((bind(server_fd, (sockaddr *)&servaddr, sizeof(servaddr))) == -1)
	{
		perror("bind");
		exit(EXIT_FAILURE);
	}

	if ((listen(server_fd, MAX_CONNECTION_QUEUE_LEN)) == -1)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	fd_to_pfd_index.emplace(server_fd, pfds.size());

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

	std::unordered_set<int> seen_fds;

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
		// TODO: Consider having a timeout of 5000 ms or something again
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
		// else if (event_count == 0)
		// {
		// 	std::cerr << "poll() timed out" << std::endl;
		// }

		seen_fds.clear();

		for (nfds_t pfd_index = pfds.size(); pfd_index > 0;)
		{
			pfd_index--;

			// If this pfd didn't have any event
			if (pfds[pfd_index].revents == 0)
			{
				continue;
			}

			int fd = pfds[pfd_index].fd;

			// If this pfd got removed
			if (fd_to_pfd_index.find(fd) == fd_to_pfd_index.end())
			{
				continue;
			}

			// If we've already iterated over this fd in this pfds loop
			if (seen_fds.find(fd) != seen_fds.end())
			{
				assert(false); // TODO: REMOVE! This is just here because I'm curious whether it happens
				continue;
			}
			seen_fds.emplace(fd);

			FdType::FdType fd_type = fd_to_fd_type.at(fd);

			printEvents(pfds[pfd_index], fd_type);

			// TODO: Try to reach this by commenting out a line that removes a closed fd from pfds
			if (pfds[pfd_index].revents & POLLNVAL)
			{
				handlePollnval();
			}

			// TODO: Previously caused by 10k_lines.txt, but how to reproduce nowadays?:
			// "This  bit  is also set for a file descriptor referring to the write end of a pipe when the read end has been closed."
			if (pfds[pfd_index].revents & POLLERR)
			{
				handlePollerr(fd);
				continue;
			}

			bool should_continue = false;

			// If the client disconnected
			if (pfds[pfd_index].revents & POLLHUP)
			{
				handlePollhup(fd, fd_type, pfd_index, should_continue);
				if (should_continue)
				{
					continue;
				}
			}

			// If we can read
			if (pfds[pfd_index].revents & POLLIN_ANY)
			{
				handlePollin(fd, fd_type, should_continue);
				if (should_continue)
				{
					continue;
				}
			}

			// If we can write
			if (pfds[pfd_index].revents & POLLOUT_ANY)
			{
				handlePollout(fd, fd_type, pfd_index);
			}
		}
	}
}

/*	Private member functions */

// TODO: Use this on every function that can fail?
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

void Server::printEvents(const pollfd &pfd, FdType::FdType fd_type)
{
	std::cerr
		<< "  fd: " << pfd.fd
		<< ", fd_type: " << fd_type
		<< ", client_index: " << (fd_type == FdType::SERVER ? -1 : fd_to_client_index.at(pfd.fd))
		<< " (with clients.size() being " << clients.size() << ")"
		<< ", client_fd: " << (fd_type == FdType::SERVER ? -1 : clients.at(fd_to_client_index.at(pfd.fd)).client_fd)
		<< ", revents:"
		<< ((pfd.revents & POLLIN) ? " POLLIN" : "")
		<< ((pfd.revents & POLLOUT) ? " POLLOUT" : "")
		<< ((pfd.revents & POLLHUP) ? " POLLHUP" : "")
		<< ((pfd.revents & POLLNVAL) ? " POLLNVAL" : "")
		<< ((pfd.revents & POLLPRI) ? " POLLPRI" : "")
		<< ((pfd.revents & POLLRDBAND) ? " POLLRDBAND" : "")
		<< ((pfd.revents & POLLRDNORM) ? " POLLRDNORM" : "")
		<< ((pfd.revents & POLLWRBAND) ? " POLLWRBAND" : "")
		<< ((pfd.revents & POLLWRNORM) ? " POLLWRNORM" : "")
		<< ((pfd.revents & POLLERR) ? " POLLERR" : "")
		<< std::endl;
}

void Server::handlePollnval(void)
{
	// TODO: Remove the client?
	// TODO: Try to reach this by commenting out a line that removes a closed fd from pfds
	assert(false);
}

void Server::handlePollerr(int fd)
{
	Client &client = getClient(fd);
	client.cgi_write_state = CGIWriteState::DONE;
	removeFd(client.server_to_cgi_fd);
}

void Server::handlePollhup(int fd, FdType::FdType fd_type, nfds_t pfd_index, bool &should_continue)
{
	// If the CGI script closed its stdout
	if (fd_type == FdType::CGI_TO_SERVER)
	{
		// If the server has read everything from the CGI script
		if (!(pfds[pfd_index].revents & POLLIN))
		{
			// TODO: REMOVE THIS!!
			// std::cerr << "Swapping pfd 1 and 2" << std::endl;
			// fd_to_pfd_index[pfds[1].fd] = 2;
			// fd_to_pfd_index[pfds[2].fd] = 1;

			// pollfd tmp = pfds[2];
			// pfds[2] = pfds[1];
			// pfds[1] = tmp;

			pollhupCGIToServer(fd);
			should_continue = true;
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
				std::cerr << "    PID " << child_pid << " exit status is " << WEXITSTATUS(child_exit_status) << std::endl;
			}
			else if (WIFSTOPPED(child_exit_status))
			{
				std::cerr << "    PID " << child_pid << " was stopped by " << WSTOPSIG(child_exit_status) << std::endl;
				assert(false); // TODO: What to do here?
			}
			else if (WIFSIGNALED(child_exit_status))
			{
				std::cerr << "    PID " << child_pid << " exited due to signal " << WTERMSIG(child_exit_status) << std::endl;

				// TODO: What to do if it receives SIGKILL?:
				// TODO: https://www.ibm.com/docs/en/aix/7.2?topic=management-process-termination

				if (WTERMSIG(child_exit_status) == SIGTERM)
				{
					// TODO: Should we do anything if its client still exists?
					should_continue = true;
					return;
				}
			}
			else
			{
				// TODO: Decide whether we want to check the remaining WCOREDUMP() and WIFCONTINUED()
				assert(false); // TODO: What to do here?
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

		should_continue = true;
	}
	else
	{
		// TODO: Should be unreachable
		assert(false);
	}
}

void Server::handlePollin(int fd, FdType::FdType fd_type, bool &should_continue)
{
	if (fd_type == FdType::SERVER)
	{
		acceptClient();
	}
	else
	{
		assert(fd_type == FdType::CLIENT || fd_type == FdType::CGI_TO_SERVER);

		Client &client = getClient(fd);

		if (!readFd(client, fd, fd_type, should_continue))
		{
			// TODO: Print error
			exit(EXIT_FAILURE);
		}
	}
}

void Server::handlePollout(int fd, FdType::FdType fd_type, nfds_t pfd_index)
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

void Server::removeClient(int fd)
{
	assert(fd != -1);
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
		std::cerr << "    Killing this client's CGI PID " << client.cgi_pid << " with SIGTERM" << std::endl;
		kill(client.cgi_pid, SIGTERM);

		cgi_pid_to_client_fd.erase(client.cgi_pid);
		client.cgi_pid = -1;
	}

	// TODO: Is it possible for client.client_fd to have already been closed and erased; check if it's -1?
	size_t client_index = fd_to_client_index.at(client.client_fd);

	fd_to_client_index[clients.back().client_fd] = client_index;
	fd_to_client_index[clients.back().server_to_cgi_fd] = client_index;
	fd_to_client_index[clients.back().cgi_to_server_fd] = client_index;
	fd_to_client_index[clients.back().cgi_exit_detector_fd] = client_index;

	removeFd(client.client_fd);
	swapRemove(clients, client_index);
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
	fd_to_fd_type.erase(fd);

	if (close(fd) == -1)
	{
		perror("close");
		exit(EXIT_FAILURE);
	}

	fd = -1;
}

void Server::pollhupCGIToServer(int fd)
{
	std::cerr << "  In pollhupCGIToServer()" << std::endl;

	Client &client = getClient(fd);

	// TODO: .erase(client.cgi_pid), and possibly also kill()/signal() it here?

	client.client_read_state = ClientReadState::DONE;

	if (client.server_to_cgi_fd != -1)
	{
		client.cgi_write_state = CGIWriteState::DONE;
		removeFd(client.server_to_cgi_fd);
	}

	// Close and remove cgi_to_server
	assert(client.cgi_to_server_fd != -1);
	removeFd(client.cgi_to_server_fd);
	client.cgi_read_state = CGIReadState::DONE;
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

bool Server::readFd(Client &client, int fd, FdType::FdType fd_type, bool &should_continue)
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
		// TODO: Probably need to send the client a response like "expected more body bytes" if it's less than content_length

		// Always true, since cgi_to_server is the only other read() caller,
		// and it raises POLLHUP rather than POLLIN on EOF, unlike client sockets
		assert(fd_type == FdType::CLIENT);

		removeClient(client.client_fd);
		should_continue = true;

		return true;
	}

	// assert(client.cgi_read_state != CGIReadState::DONE);
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

		if (client.client_read_state != ClientReadState::HEADER && !client.body.empty() && client.cgi_read_state != CGIReadState::DONE)
		{
			size_t server_to_cgi_pfds_index = fd_to_pfd_index.at(client.server_to_cgi_fd);
			std::cerr << "    Enabling server_to_cgi POLLOUT" << std::endl;
			pfds[server_to_cgi_pfds_index].events |= POLLOUT;
		}
	}
	else if (fd_type == FdType::CGI_TO_SERVER)
	{
		assert(client.cgi_read_state == CGIReadState::READING_FROM_CGI);
		std::cerr << "    Adding this substr to the response:\n----------\n" << std::string(received, bytes_read) << "\n----------\n" << std::endl;
		client.response += std::string(received, bytes_read);
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

		std::cerr << "    The child is going to start the CGI script" << std::endl;
		// TODO: Define CGI script path in the configuration file?
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

	// TODO: If this is a GET or a DELETE (can they have a body?)
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
		pfds[pfd_index].revents &= ~POLLOUT;

		if (client.client_read_state == ClientReadState::DONE)
		{
			client.cgi_write_state = CGIWriteState::DONE;

			removeFd(client.server_to_cgi_fd);
		}
	}
}

void Server::writeToClient(Client &client, int fd, nfds_t pfd_index)
{
	std::cerr << "  Writing to the client..." << std::endl;

	assert(client.client_fd == fd);
	assert(client.client_write_state == ClientWriteState::WRITING_TO_CLIENT);

	size_t max_client_write_len = MAX_CLIENT_WRITE_LEN; // TODO: Read from config; HAS to be >= 1
	size_t response_substr_len = std::min(client.response.length() - client.response_index, max_client_write_len);

	assert(response_substr_len > 0);

	// TODO: substr() can fail
	std::string response_substr = client.response.substr(client.response_index, response_substr_len);

	client.response_index += response_substr_len;

	std::cerr << "    Sending this response substr to the client that has a length of " << response_substr.length() << " bytes:\n----------\n" << response_substr << "\n----------\n" << std::endl;

	// TODO: Don't ignore errors
	write(fd, response_substr.c_str(), response_substr.length());

	// TODO: Close the client at some point
	// std::cerr << "    Closing client fd " << fd << std::endl;

	// If we don't have anything left to write at the moment
	if (client.response_index == client.response.length())
	{
		std::cerr << "    Disabling client POLLOUT" << std::endl;
		pfds[pfd_index].events &= ~POLLOUT;
		pfds[pfd_index].revents &= ~POLLOUT;
	}
}
