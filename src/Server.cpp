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

bool shutting_down_gracefully = false;

static void sigIntHandler(int signum)
{
	(void)signum;
	shutting_down_gracefully = true;
}

int Server::_sig_chld_pipe[2];

/*	Constructors */

Server::Server(void)
	: _server_fd(-1),
	  _cgi_pid_to_client_fd(),
	  _fd_to_client_index(),
	  _fd_to_pfd_index(),
	  _fd_to_fd_type(),
	  _clients(),
	  _pfds()
{
	// TODO: Parse config

	// Protocol 0 lets socket() pick a protocol, based on the requested socket type (stream)
	// Source: https://pubs.opengroup.org/onlinepubs/009695399/functions/socket.html
	if ((_server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) throw SystemException("socket");

	// Prevents "bind: Address already in use" error after:
	// 1. Starting a CGI script, 2. Doing Ctrl+\ on the server, 3. Restarting the server
	int option = 1; // "the parameter should be non-zero to enable a boolean option"
	if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1) throw SystemException("setsockopt");

	// TODO: REMOVE
	// Leave this commented out for evaluation demonstration purposes
	if (write(-1, "", 0) == -1) throw SystemException("test");

	sockaddr_in servaddr{};
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERVER_PORT);

	if ((bind(_server_fd, (sockaddr *)&servaddr, sizeof(servaddr))) == -1) throw SystemException("bind");

	if ((listen(_server_fd, MAX_CONNECTION_QUEUE_LEN)) == -1) throw SystemException("listen");

	_addFd(_server_fd, FdType::SERVER, POLLIN);
	std::cerr << "Added server fd " << _server_fd << std::endl;

	std::cerr << "Port is " << SERVER_PORT << std::endl;

	signal(SIGINT, sigIntHandler);
	signal(SIGPIPE, SIG_IGN);

	if (pipe(_sig_chld_pipe) == -1) throw SystemException("pipe");
	_addFd(_sig_chld_pipe[PIPE_READ_INDEX], FdType::SIG_CHLD, POLLIN);
	std::cerr << "Added _sig_chld_pipe[PIPE_READ_INDEX] fd " << _sig_chld_pipe[PIPE_READ_INDEX] << std::endl;
	signal(SIGCHLD, _sigChldHandler);
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
	// TODO: close() the two _sig_chld_pipe fds and such

	// TODO: close() the server socket

	// TODO: Do anything else?
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

			servers_active = false;

			// TODO: Handle multiple servers; the required steps are listed here: https://stackoverflow.com/a/15560580/13279557
			size_t server_pfd_index = 0;
			_fd_to_pfd_index[_pfds.back().fd] = server_pfd_index;
			_swapRemove(_pfds, server_pfd_index);
		}

		// If the only fd left in _pfds is _sig_chld_pipe[PIPE_READ_INDEX], return
		if (_pfds.size() == 1)
		{
			_removeFd(_sig_chld_pipe[PIPE_READ_INDEX]);
			return;
		}
		else if (shutting_down_gracefully) {
			// TODO: Do we want to use : iteration in other spots too?
			for (pollfd pfd : _pfds)
			{
				std::cerr << "  Waiting for poll fd " << pfd.fd << std::endl;
			}
		}

		std::cerr << "Waiting for an event..." << std::endl;
		// TODO: Consider having a timeout of 5000 ms or something again
		int event_count = poll(_pfds.data(), _pfds.size(), -1);
		if (event_count == -1)
		{
			if (errno == EINTR)
			{
				std::cerr << "  poll() got interrupted by a signal handler" << std::endl;
				continue;
			}
			 throw SystemException("poll");
		}
		// else if (event_count == 0)
		// {
		// 	std::cerr << "poll() timed out" << std::endl;
		// }

		_printContainerSizes();

		seen_fds.clear();

		for (nfds_t pfd_index = _pfds.size(); pfd_index > 0;)
		{
			pfd_index--;

			// If this pfd didn't have any event
			if (_pfds[pfd_index].revents == 0)
			{
				continue;
			}

			int fd = _pfds[pfd_index].fd;

			// If this pfd got removed
			if (_fd_to_pfd_index.find(fd) == _fd_to_pfd_index.end())
			{
				continue;
			}

			// If we've already iterated over this fd in this _pfds loop
			if (seen_fds.find(fd) != seen_fds.end())
			{
				assert(false); // TODO: REMOVE! This is just here because I'm curious whether it happens
				continue;
			}
			seen_fds.emplace(fd);

			FdType::FdType fd_type = _fd_to_fd_type.at(fd);

			_printEvents(_pfds[pfd_index], fd_type);

			// TODO: Try to reach this by commenting out a line that removes a closed fd from _pfds
			if (_pfds[pfd_index].revents & POLLNVAL)
			{
				_handlePollnval();
			}

			// If we are trying to write to a CGI script that closed its stdin,
			// or we are trying to write to a disconnected client
			if (_pfds[pfd_index].revents & POLLERR)
			{
				_handlePollerr(fd, fd_type);
				continue;
			}

			// If the CGI script closed its stdout
			if (_pfds[pfd_index].revents & POLLHUP)
			{
				assert(fd_type == FdType::CGI_TO_SERVER);

				// If the server has not read everything from the CGI script
				if (!(_pfds[pfd_index].revents & POLLIN))
				{
					_pollhupCGIToServer(fd);
					continue;
				}
			}

			// If we can read
			if (_pfds[pfd_index].revents & POLLIN_ANY)
			{
				bool should_continue = false;
				_handlePollin(fd, fd_type, should_continue);
				if (should_continue)
				{
					continue;
				}
			}

			// If we can write
			if (_pfds[pfd_index].revents & POLLOUT_ANY)
			{
				_handlePollout(fd, fd_type, pfd_index);
			}
		}
	}
}

/*	Private member functions */

// vector=[ A, B, C, D ] with index=1 results in [ A, D, C ]
template <typename T>
void Server::_swapRemove(T &vector, size_t index)
{
	vector[index] = vector.back();
	vector.pop_back();
}

void Server::_printContainerSizes(void)
{
	std::cerr
		<< "MAPS: "
		<< "_cgi_pid_to_client_fd=" << _cgi_pid_to_client_fd.size()
		<< ", _fd_to_client_index=" << _fd_to_client_index.size()
		<< ", _fd_to_pfd_index=" << _fd_to_pfd_index.size()
		<< ", _fd_to_fd_type=" << _fd_to_fd_type.size()
		<< " | VECTORS: "
		<< "_clients=" << _clients.size()
		<< ", _pfds=" << _pfds.size()
		<< std::endl;
}

void Server::_printEvents(const pollfd &pfd, FdType::FdType fd_type)
{
	std::cerr
		<< "  fd: " << pfd.fd
		<< ", fd_type: " << fd_type
		<< ", client_index: " << ((fd_type == FdType::SERVER || fd_type == FdType::SIG_CHLD) ? -1 : _fd_to_client_index.at(pfd.fd))
		<< ", client_fd: " << ((fd_type == FdType::SERVER || fd_type == FdType::SIG_CHLD) ? -1 : _clients.at(_fd_to_client_index.at(pfd.fd)).client_fd)
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

// TODO: Don't let _fd_to_client_index nor _clients be passed in, by just getting it from the member variable;
// TODO: Do the same for other methods that are getting useless args!
// This is called in many spots
// due to the fact that client pointer can dangle
// whenever the _clients vector resizes
Client &Server::_getClient(int fd)
{
	size_t client_index = _fd_to_client_index.at(fd);
	return _clients[client_index];
}

void Server::_removeClientFd(int &fd)
{
	assert(fd != -1);

	_fd_to_client_index.erase(fd);

	_removeFd(fd);
}

void Server::_removeFd(int &fd)
{
	assert(fd != -1);

	std::cerr << "    Removing fd " << fd << std::endl;

	size_t pfd_index = _fd_to_pfd_index.at(fd);
	_fd_to_pfd_index[_pfds.back().fd] = pfd_index;
	_swapRemove(_pfds, pfd_index);

	_fd_to_pfd_index.erase(fd);
	_fd_to_fd_type.erase(fd);

	if (close(fd) == -1) throw SystemException("close");

	fd = -1;
}

void Server::_enableEvent(size_t pfd_index, short int event)
{
	std::cerr << "    Enabling event " << event << " on pfd_index " << pfd_index << std::endl;
	_pfds[pfd_index].events |= event;
}

void Server::_disableEvent(size_t pfd_index, short int event)
{
	std::cerr << "    Disabling event " << event << " on pfd_index " << pfd_index << std::endl;
	_pfds[pfd_index].events &= ~event;
	_pfds[pfd_index].revents &= ~event;
}

void Server::_enableWritingToClient(Client &client)
{
	std::cerr << "    In _enableWritingToClient()" << std::endl;
	size_t client_pfd_index = _fd_to_pfd_index.at(client.client_fd);
	_enableEvent(client_pfd_index, POLLOUT);

	client.client_write_state = ClientWriteState::WRITING_TO_CLIENT;
}

void Server::_disableReadingFromClient(Client &client)
{
	std::cerr << "    In _disableReadingFromClient()" << std::endl;
	size_t client_pfd_index = _fd_to_pfd_index.at(client.client_fd);
	_disableEvent(client_pfd_index, POLLIN);

	client.client_read_state = ClientReadState::DONE;
}

void Server::_addClientFd(int fd, size_t client_index, FdType::FdType fd_type, short int events)
{
	assert(fd != -1);

	_fd_to_client_index.emplace(fd, client_index);

	_addFd(fd, fd_type, events);
}

void Server::_addFd(int fd, FdType::FdType fd_type, short int events)
{
	assert(fd != -1);

	std::cerr << "    Adding fd " << fd << std::endl;

	_fd_to_fd_type.emplace(fd, fd_type);
	_fd_to_pfd_index.emplace(fd, _pfds.size());

	pollfd pfd;
	pfd.fd = fd;
	pfd.events = events;
	_pfds.push_back(pfd);
}

void Server::_sigChldHandler(int signum)
{
	std::cerr << "In _sigChldHandler()" << std::endl;
	(void)signum;

	char dummy = '!';
	if (write(_sig_chld_pipe[PIPE_WRITE_INDEX], &dummy, sizeof(dummy)) == -1) throw SystemException("write");
}

void Server::_handlePollnval(void)
{
	// TODO: Remove the client?
	// TODO: Try to reach this by commenting out a line that removes a closed fd from _pfds
	assert(false);
}

void Server::_handlePollerr(int fd, FdType::FdType fd_type)
{
	if (fd_type == FdType::SERVER_TO_CGI)
	{
		Client &client = _getClient(fd);
		client.cgi_write_state = CGIWriteState::DONE;
		_removeClientFd(client.server_to_cgi_fd);
	}
	else if (fd_type == FdType::CLIENT)
	{
		_removeClient(fd);
	}
	else
	{
		// TODO: Should be unreachable
		assert(false);
	}
}

void Server::_pollhupCGIToServer(int fd)
{
	std::cerr << "  In _pollhupCGIToServer()" << std::endl;

	Client &client = _getClient(fd);

	// TODO: .erase(client.cgi_pid), and possibly also kill()/signal() it here?

	client.client_read_state = ClientReadState::DONE;

	if (client.server_to_cgi_fd != -1)
	{
		client.cgi_write_state = CGIWriteState::DONE;
		_removeClientFd(client.server_to_cgi_fd);
	}

	// Close and remove cgi_to_server
	assert(client.cgi_to_server_fd != -1);
	_removeClientFd(client.cgi_to_server_fd);
	client.cgi_read_state = CGIReadState::DONE;

	if (client.cgi_exit_status != -1)
	{
		_enableWritingToClient(client);
		client.prependResponseHeader(); // TODO: Do we need to wrap this in a ClientException try-catch?
	}
}

void Server::_handlePollin(int fd, FdType::FdType fd_type, bool &should_continue)
{
	if (fd_type == FdType::SERVER)
	{
		_acceptClient();
	}
	else if (fd_type == FdType::SIG_CHLD)
	{
		_reapChild();

		// TODO: Do we need to manually call methods here that remove server_to_cgi and cgi_to_server?

		should_continue = true;
	}
	else
	{
		assert(fd_type == FdType::CLIENT || fd_type == FdType::CGI_TO_SERVER);

		Client &client = _getClient(fd);

		try
		{
			_readFd(client, fd, fd_type, should_continue);
		}
		catch (const Client::ClientException &e)
		{
			std::cerr << "  " << e.what() << std::endl;

			client.status = e.status;

			_removeClientAttachments(fd);

			_disableReadingFromClient(client);

			_enableWritingToClient(client);

			client.response = "";
			client.response_index = 0;
			client.prependResponseHeader(); // TODO: Do we need to wrap this in a ClientException try-catch?

			should_continue = true;
		}
	}
}

void Server::_acceptClient()
{
	int client_fd = accept(_server_fd, NULL, NULL);
	std::cerr << "    Accepted client fd " << client_fd << std::endl;

	// TODO: Handle accept() failing. Specifically handle too many open fds gracefully

	_addClientFd(client_fd, _clients.size(), FdType::CLIENT, POLLIN);

	_clients.push_back(Client(client_fd));
}

void Server::_reapChild(void)
{
	std::cerr << "    In _reapChild()" << std::endl;

	char dummy;
	if (read(_sig_chld_pipe[PIPE_READ_INDEX], &dummy, 1) == -1) throw SystemException("read");

	// Reaps all children that have exited
	// waitpid() returns 0 if no more children can be reaped right now
	// WNOHANG guarantees that this call doesn't block
	// This is done in a loop, since signals aren't queued: https://stackoverflow.com/a/45809843/13279557
	// TODO: Are there other options we should use?
	int child_exit_status;
	pid_t child_pid = waitpid(-1, &child_exit_status, WNOHANG);

	// TODO: Decide what to do when errno is EINTR
	// TODO: errno is set to ECHILD when there are no children left to wait for: if (child_pid == -1 && errno != ECHILD)
	if (child_pid == -1) throw SystemException("waitpid");

	// TODO: Can this be 0 if the child was interrupted/resumes after being interrupted?
	assert(child_pid > 0);

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

		if (WTERMSIG(child_exit_status) == SIGTERM)
		{
			// TODO: Should we do anything with its client if it still exists?
			return;
		}

		// TODO: What to do if it receives for example SIGKILL?:
		// TODO: https://www.ibm.com/docs/en/aix/7.2?topic=management-process-termination
		assert(false);
	}
	else
	{
		// TODO: Decide whether we want to check the remaining WCOREDUMP() and WIFCONTINUED()
		assert(false); // TODO: What to do here?
	}

	int client_fd = _cgi_pid_to_client_fd.at(child_pid);
	size_t client_index = _fd_to_client_index.at(client_fd);
	Client &client = _clients.at(client_index);

	_cgi_pid_to_client_fd.erase(client.cgi_pid);
	client.cgi_pid = -1;

	assert(client.client_read_state == ClientReadState::DONE);
	assert(client.cgi_write_state == CGIWriteState::DONE);
	assert(client.cgi_read_state != CGIReadState::NOT_READING);
	assert(client.client_write_state == ClientWriteState::NOT_WRITING);

	if (WIFEXITED(child_exit_status))
	{
		client.cgi_exit_status = WEXITSTATUS(child_exit_status);
	}

	if (client.cgi_read_state == CGIReadState::DONE)
	{
		_enableWritingToClient(client);
		client.prependResponseHeader(); // TODO: Do we need to wrap this in a ClientException try-catch?
	}
}

void Server::_readFd(Client &client, int fd, FdType::FdType fd_type, bool &should_continue)
{
	char received[MAX_RECEIVED_LEN] = {};

	std::cerr << "    About to call read(" << fd << ", received, " << MAX_RECEIVED_LEN << ") on fd_type " << fd_type << std::endl;

	// TODO: We should never read past the content_length of the BODY
	ssize_t bytes_read = read(fd, received, MAX_RECEIVED_LEN);
	if (bytes_read == -1) throw SystemException("read");
	if (bytes_read == 0)
	{
		std::cerr << "    Read 0 bytes" << std::endl;

		// TODO: Assert that we reached content_length
		// TODO: Probably need to send the client a response like "expected more body bytes" if it's less than content_length

		// Always true, since cgi_to_server is the only other read() caller,
		// and it raises POLLHUP rather than POLLIN on EOF, unlike client sockets
		assert(fd_type == FdType::CLIENT);

		_removeClient(client.client_fd);
		should_continue = true;

		return;
	}

	// assert(client.cgi_read_state != CGIReadState::DONE);
	assert(client.client_write_state != ClientWriteState::DONE);

	std::cerr << "    Read " << bytes_read << " bytes:\n----------\n" << std::string(received, bytes_read) << "\n----------\n" << std::endl;

	if (fd_type == FdType::CLIENT)
	{
		ClientReadState::ClientReadState previous_read_state = client.client_read_state;

		client.appendReadString(received, bytes_read);

		// If we've just started reading/entirely read this client's body, start a CGI script
		if (previous_read_state == ClientReadState::HEADER && client.client_read_state != ClientReadState::HEADER)
		{
			// TODO: Only run the below code if the request wants to start the CGI AND it is a POST request

			_startCGI(client, fd, fd_type);
		}

		if (client.client_read_state != ClientReadState::HEADER && !client.body.empty() && client.cgi_write_state != CGIWriteState::DONE)
		{
			assert(client.server_to_cgi_fd != -1);
			size_t server_to_cgi_pfd_index = _fd_to_pfd_index.at(client.server_to_cgi_fd);
			std::cerr << "    Enabling server_to_cgi POLLOUT" << std::endl;
			_enableEvent(server_to_cgi_pfd_index, POLLOUT);
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
}

void Server::_removeClient(int fd)
{
	assert(fd != -1);
	std::cerr << "  Removing client with fd " << fd << std::endl;

	_removeClientAttachments(fd);

	Client &client = _getClient(fd);

	// TODO: Is it possible for client.client_fd to have already been closed and erased; check if it's -1?
	size_t client_index = _fd_to_client_index.at(client.client_fd);

	_fd_to_client_index[_clients.back().client_fd] = client_index;
	_fd_to_client_index[_clients.back().server_to_cgi_fd] = client_index;
	_fd_to_client_index[_clients.back().cgi_to_server_fd] = client_index;

	_removeClientFd(client.client_fd);
	_swapRemove(_clients, client_index);
}

void Server::_removeClientAttachments(int fd)
{
	std::cerr << "  Removing client attachments with fd " << fd << std::endl;

	Client &client = _getClient(fd);

	if (client.server_to_cgi_fd != -1)
	{
		_removeClientFd(client.server_to_cgi_fd);
		client.cgi_write_state = CGIWriteState::DONE;
	}

	if (client.cgi_to_server_fd != -1)
	{
		_removeClientFd(client.cgi_to_server_fd);
		client.cgi_read_state = CGIReadState::DONE;
	}

	if (client.cgi_pid != -1)
	{
		std::cerr << "    Sending SIGTERM to this client's CGI script with PID " << client.cgi_pid << std::endl;
		// TODO: Isn't there a race condition here, as the cgi process may have ended and we'll still try to kill it?
		if (kill(client.cgi_pid, SIGTERM) == -1) throw SystemException("kill");

		_cgi_pid_to_client_fd.erase(client.cgi_pid);
		client.cgi_pid = -1;
	}
}

void Server::_startCGI(Client &client, int fd, FdType::FdType fd_type)
{
	std::cerr << "  Starting CGI..." << std::endl;

	assert(fd_type == FdType::CLIENT);

	int server_to_cgi_pipe[2];
	int cgi_to_server_pipe[2];

	if (pipe(server_to_cgi_pipe) == -1) throw SystemException("pipe");
	if (pipe(cgi_to_server_pipe) == -1) throw SystemException("pipe");

	pid_t forked_pid = fork();
	if (forked_pid == -1) throw SystemException("fork");
	else if (forked_pid == CHILD)
	{
		if (signal(SIGINT, SIG_IGN) == SIG_ERR) throw SystemException("signal");

		if (close(server_to_cgi_pipe[PIPE_WRITE_INDEX]) == -1) throw SystemException("close");
		if (close(cgi_to_server_pipe[PIPE_READ_INDEX]) == -1) throw SystemException("close");

		if (dup2(server_to_cgi_pipe[PIPE_READ_INDEX], STDIN_FILENO) == -1) throw SystemException("dup2");
		if (close(server_to_cgi_pipe[PIPE_READ_INDEX]) == -1) throw SystemException("close");

		if (dup2(cgi_to_server_pipe[PIPE_WRITE_INDEX], STDOUT_FILENO) == -1) throw SystemException("dup2");
		if (close(cgi_to_server_pipe[PIPE_WRITE_INDEX]) == -1) throw SystemException("close");

		std::cerr << "    The child is going to start the CGI script" << std::endl;
		// TODO: Define CGI script path in the configuration file?
		const char *path = "/usr/bin/python3";
		char *const argv[] = {(char *)"python3", (char *)"cgi-bin/print.py", NULL};

		// TODO: Construct cgi_env using header_map
		char *cgi_env[] = {NULL};
		execve(path, argv, cgi_env);

		throw SystemException("execve");
	}

	if (close(server_to_cgi_pipe[PIPE_READ_INDEX]) == -1) throw SystemException("close");
	if (close(cgi_to_server_pipe[PIPE_WRITE_INDEX]) == -1) throw SystemException("close");

	client.cgi_pid = forked_pid;

	_cgi_pid_to_client_fd.emplace(forked_pid, fd);

	size_t client_index = _fd_to_client_index.at(fd);

	int server_to_cgi_fd = server_to_cgi_pipe[PIPE_WRITE_INDEX];

	if (client.request_method == "POST")
	{
		_addClientFd(server_to_cgi_fd, client_index, FdType::SERVER_TO_CGI, client.body.empty() ? 0 : POLLOUT);
		client.server_to_cgi_fd = server_to_cgi_fd;
		client.cgi_write_state = CGIWriteState::WRITING_TO_CGI;
		std::cerr << "    Added server_to_cgi fd " << server_to_cgi_fd << std::endl;
	}
	// If this is a GET or a DELETE (TODO: can they have a body?)
	else
	{
		std::cerr << "    Closing server_to_cgi fd immediately, since there is no body" << std::endl;
		if (close(server_to_cgi_fd) == -1) throw SystemException("close");
		client.cgi_write_state = CGIWriteState::DONE;
	}

	int cgi_to_server_fd = cgi_to_server_pipe[PIPE_READ_INDEX];
	_addClientFd(cgi_to_server_fd, client_index, FdType::CGI_TO_SERVER, POLLIN);
	client.cgi_to_server_fd = cgi_to_server_fd;
	client.cgi_read_state = CGIReadState::READING_FROM_CGI;
	std::cerr << "    Added cgi_to_server fd " << cgi_to_server_fd << std::endl;
}

void Server::_handlePollout(int fd, FdType::FdType fd_type, nfds_t pfd_index)
{
	Client &client = _getClient(fd);

	if (fd_type == FdType::SERVER_TO_CGI)
	{
		_writeToCGI(client, pfd_index);
	}
	else if (fd_type == FdType::CLIENT)
	{
		_writeToClient(client, fd);
	}
	else
	{
		// TODO: Should be unreachable
		assert(false);
	}
}

void Server::_writeToCGI(Client &client, nfds_t pfd_index)
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

	if (write(client.server_to_cgi_fd, body_substr.c_str(), body_substr.length()) == -1)
	{
		// Happens when the CGI script closed its stdin
		std::cerr << "    write() detected 'Broken pipe'" << std::endl;

		std::cerr << "    Disabling server_to_cgi POLLOUT" << std::endl;
		_disableEvent(pfd_index, POLLOUT);

		client.cgi_write_state = CGIWriteState::DONE;
		_removeClientFd(client.server_to_cgi_fd);

		return;
	}

	// If we don't have anything left to write at the moment
	if (client.body_index == client.body.length())
	{
		std::cerr << "    Disabling server_to_cgi POLLOUT" << std::endl;
		_disableEvent(pfd_index, POLLOUT);

		if (client.client_read_state == ClientReadState::DONE)
		{
			client.cgi_write_state = CGIWriteState::DONE;
			_removeClientFd(client.server_to_cgi_fd);
		}
	}
}

void Server::_writeToClient(Client &client, int fd)
{
	std::cerr << "  Writing to the client..." << std::endl;

	assert(client.client_fd == fd);
	assert(client.client_write_state == ClientWriteState::WRITING_TO_CLIENT);

	size_t max_client_write_len = MAX_CLIENT_WRITE_LEN; // TODO: Read from config; HAS to be >= 1
	size_t response_substr_len = std::min(client.response.length() - client.response_index, max_client_write_len);

	assert(response_substr_len > 0);

	std::string response_substr = client.response.substr(client.response_index, response_substr_len);

	client.response_index += response_substr_len;

	std::cerr << "    Sending this response substr to the client that has a length of " << response_substr.length() << " bytes:\n----------\n" << response_substr << "\n----------\n" << std::endl;

	// sleep(5); // TODO: REMOVE

	if (write(fd, response_substr.c_str(), response_substr.length()) == -1)
	{
		// TODO: Remove the client immediately?
		// TODO: This should be reachable by commenting out the time.sleep(5) in print.py
		assert(false);
	}

	// sleep(5); // TODO: REMOVE

	if (client.response_index == client.response.length())
	{
		// TODO: Finish this commented out code
		// If this is a CGI request
		// if ()
		// {
			// Remove the client once we've sent the entire response
			_removeClient(client.client_fd);
		// }
		// // If this isn't a CGI request
		// else
		// {
		// 	// If TODO: ??
		// 	if ()
		// 	{
		// 		// Remove the client once we've sent the entire response
		// 		_removeClient(client.client_fd);
		// 	}
		// 	else
		// 	{
		// 		std::cerr << "    Disabling client POLLOUT" << std::endl;
		// 		_disableEvent(pfd_index, POLLOUT);
		// 	}
		// }
	}
}
