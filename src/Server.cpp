#include "Server.hpp"

#include "config/Config.hpp"
#include "Client.hpp"
#include "defines.hpp"
#include "Logger.hpp"
#include "Utils.hpp"

#include <cassert>
#include <iostream>
#include <filesystem>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unordered_set>

// TODO: Turn these into static ints inside of a class?
const int POLLIN_ANY = POLLIN | POLLRDBAND | POLLRDNORM | POLLPRI;
const int POLLOUT_ANY = POLLOUT | POLLWRBAND | POLLWRNORM;

bool shutting_down_gracefully = false;

int Server::_sig_chld_pipe[2];

/*	Constructors */

Server::Server(const Config &config)
	: _config(config),
	  _bind_fd_to_server_indices(),
	  _bind_fd_to_port(),
	  _cgi_pid_to_client_fd(),
	  _fd_to_client_index(),
	  _fd_to_pfd_index(),
	  _fd_to_fd_type(),
	  _clients(),
	  _pfds()
{
	for (const auto &it : _config.bind_info_to_server_indices)
	{
		const auto &bind_info = it.first;
		const auto &server_indices = it.second;

		protoent *proto = getprotobyname("tcp");
		if (proto == NULL) throw Utils::SystemException("getprotobyname");
		int bind_fd;
		if ((bind_fd = socket(AF_INET, SOCK_STREAM, proto->p_proto)) == -1) throw Utils::SystemException("socket");

		_bind_fd_to_server_indices.emplace(bind_fd, server_indices);

		Logger::info(std::string("Adding address ") + std::to_string(bind_info.s_addr) + " with port " + std::to_string(ntohs(bind_info.sin_port)));
		_bind_fd_to_port.emplace(bind_fd, std::to_string(ntohs(bind_info.sin_port)));

		// Prevents "bind: Address already in use" error
		int option = 1; // "the parameter should be non-zero to enable a boolean option"
		if (setsockopt(bind_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1) throw Utils::SystemException("setsockopt");

		sockaddr_in sockaddrin_entry{};
		sockaddrin_entry.sin_family = AF_INET;
		sockaddrin_entry.sin_addr.s_addr = bind_info.s_addr;
		sockaddrin_entry.sin_port = bind_info.sin_port;
		sockaddr *sockaddr_entry = reinterpret_cast<sockaddr *>(&sockaddrin_entry);

		if (bind(bind_fd, sockaddr_entry, sizeof(sockaddrin_entry)) == -1) throw Utils::SystemException("bind");

		if (listen(bind_fd, _config.connection_queue_length) == -1) throw Utils::SystemException("listen");

		_addFd(bind_fd, FdType::SERVER, POLLIN);

		Logger::info(std::string("Added bind fd ") + std::to_string(bind_fd));
	}

	signal(SIGINT, _sigIntHandler);
	signal(SIGPIPE, SIG_IGN);

	// Demonstration purposes
	// if (write(-1, "", 0)) throw Utils::SystemException("write");

	if (pipe(_sig_chld_pipe) == -1) throw Utils::SystemException("pipe");
	_addFd(_sig_chld_pipe[PIPE_READ_INDEX], FdType::SIG_CHLD, POLLIN);
	Logger::info(std::string("Added _sig_chld_pipe[PIPE_READ_INDEX] fd ") + std::to_string(_sig_chld_pipe[PIPE_READ_INDEX]));
	signal(SIGCHLD, _sigChldHandler);
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
			Logger::info("\nShutting down gracefully...");

			servers_active = false;

			for (const auto &fd_to_fd_type_pair : _fd_to_fd_type)
			{
				if (fd_to_fd_type_pair.second == FdType::SERVER)
				{
					int fd = fd_to_fd_type_pair.first;

					size_t pfd_index = _fd_to_pfd_index.at(fd);
					_fd_to_pfd_index[_pfds.back().fd] = pfd_index;
					_swapRemove(_pfds, pfd_index);

					_fd_to_pfd_index.erase(fd);
					// We're not erasing from _fd_to_fd_type, since we're looping through it

					if (close(fd) == -1) throw Utils::SystemException("close");
				}
			}
		}

		// If the only fd left in _pfds is _sig_chld_pipe[PIPE_READ_INDEX], return
		if (_pfds.size() == 1)
		{
			_removeFd(_sig_chld_pipe[PIPE_READ_INDEX]);
			Logger::info(std::string("Gootbye"));
			return;
		}
		else if (shutting_down_gracefully)
		{
			// TODO: Do we want to use : iteration in other spots too?
			for (pollfd pfd : _pfds)
			{
				Logger::info(std::string("  Waiting for poll fd ") + std::to_string(pfd.fd));
			}
		}

		Logger::info(std::string("Waiting for an event..."));
		// TODO: Consider having a timeout of 5000 ms or something again
		int event_count = poll(_pfds.data(), _pfds.size(), -1);
		if (event_count == -1)
		{
			if (errno == EINTR)
			{
				Logger::info(std::string("  poll() got interrupted by a signal handler"));
				continue;
			}
			throw Utils::SystemException("poll");
		}
		// else if (event_count == 0)
		// {
		// 	Logger::info(std::string("poll() timed out"));
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
			if (!_fd_to_pfd_index.contains(fd))
			{
				continue;
			}

			// If we've already iterated over this fd in this _pfds loop
			if (seen_fds.contains(fd))
			{
				assert(false); // TODO: REMOVE! This is just here because I'm curious whether it happens
				continue;
			}
			seen_fds.emplace(fd);

			FdType fd_type = _fd_to_fd_type.at(fd);

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
				bool skip_client = false;
				_handlePollin(fd, fd_type, skip_client);
				if (skip_client)
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
		<< "_bind_fd_to_server_indices=" << _bind_fd_to_server_indices.size()
		<< ", _bind_fd_to_port=" << _bind_fd_to_port.size()
		<< ", _cgi_pid_to_client_fd=" << _cgi_pid_to_client_fd.size()
		<< ", _fd_to_client_index=" << _fd_to_client_index.size()
		<< ", _fd_to_pfd_index=" << _fd_to_pfd_index.size()
		<< ", _fd_to_fd_type=" << _fd_to_fd_type.size()
		<< " | VECTORS: "
		<< "_clients=" << _clients.size()
		<< ", _pfds=" << _pfds.size()
		<< std::endl;
}

void Server::_printEvents(const pollfd &pfd, FdType fd_type)
{
	std::cerr
		<< "  fd: " << pfd.fd
		<< ", fd_type: " << static_cast<int>(fd_type)
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

	Logger::info(std::string("    Removing fd ") + std::to_string(fd));

	size_t pfd_index = _fd_to_pfd_index.at(fd);
	_fd_to_pfd_index[_pfds.back().fd] = pfd_index;
	_swapRemove(_pfds, pfd_index);

	_fd_to_pfd_index.erase(fd);
	_fd_to_fd_type.erase(fd);

	if (close(fd) == -1) throw Utils::SystemException("close");

	fd = -1;
}

void Server::_enableEvent(size_t pfd_index, short int event)
{
	Logger::info(std::string("    Enabling event ") + std::to_string(event) + " on pfd_index " + std::to_string(pfd_index));
	_pfds[pfd_index].events |= event;
}

void Server::_disableEvent(size_t pfd_index, short int event)
{
	Logger::info(std::string("    Disabling event ") + std::to_string(event) + " on pfd_index " + std::to_string(pfd_index));
	_pfds[pfd_index].events &= ~event;
	_pfds[pfd_index].revents &= ~event;
}

void Server::_enableWritingToClient(Client &client)
{
	// TODO: Assert that response isn't empty?

	Logger::info(std::string("    In _enableWritingToClient()"));
	size_t client_pfd_index = _fd_to_pfd_index.at(client.client_fd);
	_enableEvent(client_pfd_index, POLLOUT);

	client.client_write_state = Client::ServerToClientState::WRITING;
}

void Server::_enableWritingToCGI(Client &client)
{
	// TODO: Assert that response isn't empty?

	assert(client.cgi_write_state != Client::ServerToCGIState::DONE);
	assert(client.server_to_cgi_fd != -1);

	Logger::info(std::string("    In _enableWritingToCGI()"));
	size_t server_to_cgi_pfd_index = _fd_to_pfd_index.at(client.server_to_cgi_fd);
	_enableEvent(server_to_cgi_pfd_index, POLLOUT);

	client.cgi_write_state = Client::ServerToCGIState::WRITING;
}

void Server::_disableReadingFromClient(Client &client)
{
	Logger::info(std::string("    In _disableReadingFromClient()"));
	size_t client_pfd_index = _fd_to_pfd_index.at(client.client_fd);
	_disableEvent(client_pfd_index, POLLIN);

	client.client_read_state = Client::ClientToServerState::DONE;
}

void Server::_addClientFd(int fd, size_t client_index, FdType fd_type, short int events)
{
	assert(fd != -1);

	_fd_to_client_index.emplace(fd, client_index);

	_addFd(fd, fd_type, events);
}

void Server::_addFd(int fd, FdType fd_type, short int events)
{
	assert(fd != -1);

	Logger::info(std::string("    Adding fd ") + std::to_string(fd));

	_fd_to_fd_type.emplace(fd, fd_type);
	_fd_to_pfd_index.emplace(fd, _pfds.size());

	pollfd pfd;
	pfd.fd = fd;
	pfd.events = events;
	_pfds.push_back(pfd);
}

void Server::_sigIntHandler(int signum)
{
	(void)signum;
	shutting_down_gracefully = true;
}

void Server::_sigChldHandler(int signum)
{
	(void)signum;
	Logger::info(std::string("In _sigChldHandler()"));

	char dummy = '!';
	if (write(_sig_chld_pipe[PIPE_WRITE_INDEX], &dummy, sizeof(dummy)) == -1) throw Utils::SystemException("write");
}

void Server::_handlePollnval(void)
{
	// TODO: Remove the client?
	// TODO: Try to reach this by commenting out a line that removes a closed fd from _pfds
	assert(false);
}

void Server::_handlePollerr(int fd, FdType fd_type)
{
	if (fd_type == FdType::SERVER_TO_CGI)
	{
		Client &client = _getClient(fd);
		client.cgi_write_state = Client::ServerToCGIState::DONE;
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
	Logger::info(std::string("  In _pollhupCGIToServer()"));

	Client &client = _getClient(fd);

	// TODO: .erase(client.cgi_pid), and possibly also kill()/signal() it here?

	client.client_read_state = Client::ClientToServerState::DONE;

	if (client.server_to_cgi_fd != -1)
	{
		client.cgi_write_state = Client::ServerToCGIState::DONE;
		_removeClientFd(client.server_to_cgi_fd);
	}

	// Close and remove cgi_to_server
	assert(client.cgi_to_server_fd != -1);
	_removeClientFd(client.cgi_to_server_fd);
	client.cgi_read_state = Client::CGIToServerState::DONE;

	if (client.cgi_exit_status != -1)
	{
		_enableWritingToClient(client);
		client.prependResponseHeader(); // TODO: Do we need to wrap this in a ClientException try-catch?
	}
}

void Server::_handlePollin(int fd, FdType fd_type, bool &skip_client)
{
	if (fd_type == FdType::SERVER)
	{
		_acceptClient(fd);
	}
	else
	{
		if (fd_type == FdType::SIG_CHLD)
		{
			_reapChild();

			// TODO: Do we need to manually call methods here that remove server_to_cgi and cgi_to_server?

			skip_client = true;
		}
		else
		{
			assert(fd_type == FdType::CLIENT || fd_type == FdType::CGI_TO_SERVER);

			Client &client = _getClient(fd);

			try
			{
				_readFd(client, fd, fd_type, skip_client);
			}
			catch (const Client::ClientException &e)
			{
				_handleClientException(e, client);

				skip_client = true;
			}
		}
	}
}

void Server::_acceptClient(int server_fd)
{
	int client_fd = accept(server_fd, NULL, NULL);
	Logger::info(std::string("    Accepted client fd ") + std::to_string(client_fd));

	// TODO: Handle accept() failing. Specifically handle too many open fds gracefully

	_addClientFd(client_fd, _clients.size(), FdType::CLIENT, POLLIN);

	const std::string &server_port = _bind_fd_to_port.at(server_fd);

	_clients.push_back(Client(client_fd, server_fd, server_port, _config.client_max_body_size));
}

void Server::_reapChild(void)
{
	Logger::info(std::string("    In _reapChild()"));

	char dummy;
	if (read(_sig_chld_pipe[PIPE_READ_INDEX], &dummy, 1) == -1) throw Utils::SystemException("read");

	// Reaps all children that have exited
	// waitpid() returns 0 if no more children can be reaped right now
	// WNOHANG guarantees that this call doesn't block
	// This is done in a loop, since signals aren't queued: https://stackoverflow.com/a/45809843/13279557
	// TODO: Are there other options we should use?
	int child_exit_status;
	pid_t child_pid = waitpid(-1, &child_exit_status, WNOHANG);

	// TODO: Decide what to do when errno is EINTR
	// TODO: errno is set to ECHILD when there are no children left to wait for: if (child_pid == -1 && errno != ECHILD)
	if (child_pid == -1) throw Utils::SystemException("waitpid");

	// TODO: Can this be 0 if the child was interrupted/resumes after being interrupted?
	assert(child_pid > 0);

	// Reached when the client disconnects before the CGI has finished
	if (!_cgi_pid_to_client_fd.contains(child_pid))
	{
		return;
	}

	int client_fd = _cgi_pid_to_client_fd.at(child_pid);
	size_t client_index = _fd_to_client_index.at(client_fd);
	Client &client = _clients.at(client_index);

	_cgi_pid_to_client_fd.erase(client.cgi_pid);
	client.cgi_pid = -1;

	assert(client.client_read_state == Client::ClientToServerState::DONE);
	assert(client.cgi_write_state == Client::ServerToCGIState::DONE);
	assert(client.cgi_read_state != Client::CGIToServerState::NOT_READING);
	assert(client.client_write_state == Client::ServerToClientState::NOT_WRITING);

	if (WIFEXITED(child_exit_status))
	{
		client.cgi_exit_status = WEXITSTATUS(child_exit_status);
	}

	if (client.cgi_read_state == Client::CGIToServerState::DONE)
	{
		_enableWritingToClient(client);
		client.prependResponseHeader(); // TODO: Do we need to wrap this in a ClientException try-catch?
	}

	bool cgi_exit_ok = WIFEXITED(child_exit_status) && WEXITSTATUS(child_exit_status) == 0;
	if (!cgi_exit_ok)
	{
		_handleClientException(Client::ClientException(Status::INTERNAL_SERVER_ERROR), client);
	}
}

void Server::_readFd(Client &client, int fd, FdType fd_type, bool &skip_client)
{
	char received[MAX_RECEIVED_LEN] = {};

	Logger::info(std::string("    About to call read(") + std::to_string(fd) + ", received, " + std::to_string(MAX_RECEIVED_LEN) + ") on fd_type " + std::to_string(static_cast<int>(fd_type)));

	// TODO: We should never read past the content_length of the BODY
	ssize_t bytes_read = read(fd, received, MAX_RECEIVED_LEN);
	if (bytes_read == -1) throw Utils::SystemException("read");
	if (bytes_read == 0)
	{
		Logger::info(std::string("    Read 0 bytes"));

		// TODO: Assert that we reached content_length
		// TODO: Probably need to send the client a response like "expected more body bytes" if it's less than content_length

		// Always true, since cgi_to_server is the only other read() caller,
		// and it raises POLLHUP rather than POLLIN on EOF, unlike client sockets
		assert(fd_type == FdType::CLIENT);

		_removeClient(client.client_fd);
		skip_client = true;

		return;
	}

	// assert(client.cgi_read_state != Client::CGIToServerState::DONE);
	assert(client.client_write_state != Client::ServerToClientState::DONE);

	Logger::info(std::string("    Read ") + std::to_string(bytes_read) + " bytes:\n----------\n" + std::string(received, bytes_read) + "\n----------\n");

	if (fd_type == FdType::CLIENT)
	{
		client.appendReadString(received, bytes_read);

		if (client.client_read_state == Client::ClientToServerState::DONE)
		{
			const std::string &target = client.request_target;
			const std::string &method = client.request_method;

			size_t server_index = _getServerIndexFromClientServerName(client);
			const Config::ServerDirective &server = _config.servers.at(server_index);

			const ResolvedLocation location = _resolveToLocation(target, server);

			Logger::info(std::string("    location.path: '") + location.path + "'");

			if (!location.resolved)
			{
				throw Client::ClientException(Status::NOT_FOUND);
			}
			if (location.has_redirect)
			{
				client.redirect = location.path;
				throw Client::ClientException(Status::MOVED_TEMPORARILY);
			}
			if (!_isAllowedMethod(location, method))
			{
				throw Client::ClientException(Status::METHOD_NOT_ALLOWED);
			}

			if (location.path.back() == '/')
			{
				if (method == "GET")
				{
					if (location.has_index)
					{
						client.respondWithFile(location.index_path);
						_enableWritingToClient(client);
					}
					else if (location.autoindex)
					{
						client.respondWithDirectoryListing(location.path);
						_enableWritingToClient(client);
					}
					else
					{
						throw Client::ClientException(Status::FORBIDDEN);
					}
				}
				else
				{
					throw Client::ClientException(Status::FORBIDDEN);
				}
			}
			else
			{
				if (std::filesystem::is_directory(location.path))
				{
					if (method == "DELETE")
					{
						throw Client::ClientException(Status::METHOD_NOT_ALLOWED);
					}
					else
					{
						throw Client::ClientException(Status::MOVED_PERMANENTLY);
					}
				}

				if (location.is_cgi_directory)
				{
					_startCGI(client, location.cgi_settings, location.script_name, location.path_info, location.query_string);
				}
				else if (method == "GET")
				{
					client.respondWithFile(location.path);
					_enableWritingToClient(client);
				}
				else if (method == "POST")
				{
					client.respondWithCreateFile(location.path);
					_enableWritingToClient(client);
				}
				else
				{
					client.respondWithDeleteFile(location.path);
					_enableWritingToClient(client);
				}
			}
		}
	}
	else if (fd_type == FdType::CGI_TO_SERVER)
	{
		assert(client.cgi_read_state == Client::CGIToServerState::READING);
		Logger::info(std::string("    Adding this substr to the response:\n----------\n") + std::string(received, bytes_read) + "\n----------\n");
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
	Logger::info(std::string("  Removing client with fd ") + std::to_string(fd));

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
	Logger::info(std::string("  Removing client attachments with fd ") + std::to_string(fd));

	Client &client = _getClient(fd);

	if (client.server_to_cgi_fd != -1)
	{
		_removeClientFd(client.server_to_cgi_fd);
		client.cgi_write_state = Client::ServerToCGIState::DONE;
	}

	if (client.cgi_to_server_fd != -1)
	{
		_removeClientFd(client.cgi_to_server_fd);
		client.cgi_read_state = Client::CGIToServerState::DONE;
	}

	if (client.cgi_pid != -1)
	{
		Logger::info(std::string("    Sending SIGTERM to this client's CGI script with PID ") + std::to_string(client.cgi_pid));
		// TODO: Isn't there a race condition here, as the cgi process may have ended and we'll still try to kill it?
		if (kill(client.cgi_pid, SIGTERM) == -1) throw Utils::SystemException("kill");

		_cgi_pid_to_client_fd.erase(client.cgi_pid);
		client.cgi_pid = -1;
	}
}

void Server::_startCGI(Client &client, const Config::CGISettingsDirective &cgi_settings, const std::string &script_name, const std::string &path_info, const std::string &query_string)
{
	Logger::info(std::string("  Starting CGI..."));

	int server_to_cgi_pipe[2];
	int cgi_to_server_pipe[2];

	if (pipe(server_to_cgi_pipe) == -1) throw Utils::SystemException("pipe");
	if (pipe(cgi_to_server_pipe) == -1) throw Utils::SystemException("pipe");

	pid_t forked_pid = fork();
	if (forked_pid == -1) throw Utils::SystemException("fork");
	else if (forked_pid == CHILD)
	{
		if (signal(SIGINT, SIG_IGN) == SIG_ERR) throw Utils::SystemException("signal");

		if (close(server_to_cgi_pipe[PIPE_WRITE_INDEX]) == -1) throw Utils::SystemException("close");
		if (close(cgi_to_server_pipe[PIPE_READ_INDEX]) == -1) throw Utils::SystemException("close");

		if (dup2(server_to_cgi_pipe[PIPE_READ_INDEX], STDIN_FILENO) == -1) throw Utils::SystemException("dup2");
		if (close(server_to_cgi_pipe[PIPE_READ_INDEX]) == -1) throw Utils::SystemException("close");

		if (dup2(cgi_to_server_pipe[PIPE_WRITE_INDEX], STDOUT_FILENO) == -1) throw Utils::SystemException("dup2");
		if (close(cgi_to_server_pipe[PIPE_WRITE_INDEX]) == -1) throw Utils::SystemException("close");

		_execveChild(client, cgi_settings, script_name, path_info, query_string);
	}

	if (close(server_to_cgi_pipe[PIPE_READ_INDEX]) == -1) throw Utils::SystemException("close");
	if (close(cgi_to_server_pipe[PIPE_WRITE_INDEX]) == -1) throw Utils::SystemException("close");

	client.cgi_pid = forked_pid;

	_cgi_pid_to_client_fd.emplace(forked_pid, client.client_fd);

	int server_to_cgi_fd = server_to_cgi_pipe[PIPE_WRITE_INDEX];

	size_t client_index = _fd_to_client_index.at(client.client_fd);

	if (client.cgi_write_state == Client::ServerToCGIState::DONE)
	{
		Logger::info(std::string("    Closing server_to_cgi fd immediately, since there is no body"));
		if (close(server_to_cgi_fd) == -1) throw Utils::SystemException("close");
	}
	else
	{
		_addClientFd(server_to_cgi_fd, client_index, FdType::SERVER_TO_CGI, client.body.empty() ? 0 : POLLOUT);
		client.server_to_cgi_fd = server_to_cgi_fd;
		client.cgi_write_state = Client::ServerToCGIState::WRITING;
		_enableWritingToCGI(client);
		Logger::info(std::string("    Added server_to_cgi fd ") + std::to_string(server_to_cgi_fd));
	}

	int cgi_to_server_fd = cgi_to_server_pipe[PIPE_READ_INDEX];
	_addClientFd(cgi_to_server_fd, client_index, FdType::CGI_TO_SERVER, POLLIN);
	client.cgi_to_server_fd = cgi_to_server_fd;
	client.cgi_read_state = Client::CGIToServerState::READING;
	Logger::info(std::string("    Added cgi_to_server fd ") + std::to_string(cgi_to_server_fd));
}

void Server::_execveChild(Client &client, const Config::CGISettingsDirective &cgi_settings, const std::string &script_name, const std::string &path_info, const std::string &query_string)
{
	char *argv0 = const_cast<char *>(cgi_settings.cgi_execve_argv0.c_str());
	char *argv1 = const_cast<char *>(script_name.c_str());
	char *argv2 = const_cast<char *>(path_info.c_str());

	char *const argv[] = {argv0, argv1, argv2, NULL};

	std::vector<std::string> cgi_headers;

	for (const auto &it : client.headers)
	{
		cgi_headers.push_back("HTTP_" + it.first + "=" + it.second);
	}

	cgi_headers.push_back("REQUEST_METHOD=" + client.request_method);
	cgi_headers.push_back("SCRIPT_NAME=" + script_name);
	cgi_headers.push_back("PATH_INFO=" + path_info);
	cgi_headers.push_back("QUERY_STRING=" + query_string);
	cgi_headers.push_back("CONTENT_LENGTH=" + std::to_string(client.content_length));
	cgi_headers.push_back("CONTENT_TYPE=" + client.content_type);
	cgi_headers.push_back("SERVER_NAME=" + client.server_name);
	cgi_headers.push_back("SERVER_PORT=" + client.server_port);
	cgi_headers.push_back("GATEWAY_INTERFACE=CGI/1.1");
	cgi_headers.push_back("PATH_TRANSLATED=" + std::string(std::filesystem::current_path()) + path_info);
	cgi_headers.push_back("SERVER_PROTOCOL=HTTP/1.1");

	// TODO: Give some of these values?
	cgi_headers.push_back("AUTH_TYPE=");
	cgi_headers.push_back("REMOTE_ADDR=");
	cgi_headers.push_back("REMOTE_HOST=");
	cgi_headers.push_back("REMOTE_IDENT=");
	cgi_headers.push_back("REMOTE_USER=");
	cgi_headers.push_back("SERVER_SOFTWARE="); // TODO: Share this with the HTTP response?

	const auto &cgi_env_vec = _getCGIEnv(cgi_headers);
	char **cgi_env = const_cast<char **>(cgi_env_vec.data());

	execve(cgi_settings.cgi_execve_path.c_str(), argv, cgi_env);

	// This gets turned into a ClientException by the parent process
	throw Utils::SystemException("execve");
}

std::vector<const char *> Server::_getCGIEnv(const std::vector<std::string> &cgi_headers)
{
	std::vector<const char *> cgi_env;

	for (const std::string &cgi_header : cgi_headers)
	{
		cgi_env.push_back(cgi_header.c_str());
	}

	cgi_env.push_back(NULL);

	return cgi_env;
}

Server::ResolvedLocation Server::_resolveToLocation(const std::string &request_target, const Config::ServerDirective &server)
{
	ResolvedLocation resolved{};

	size_t longest_uri_length = 0;

	for (const Config::LocationDirective &location : server.locations)
	{
		if (Utils::startsWith(request_target, location.uri) && location.uri.length() > longest_uri_length)
		{
			resolved.resolved = true;

			longest_uri_length = location.uri.length();

			resolved.is_cgi_directory = location.is_cgi_directory;
			resolved.cgi_settings = location.cgi_settings;

			resolved.script_name = "";
			resolved.path_info = "";
			resolved.query_string = "";

			resolved.has_index = !location.index.empty();
			resolved.autoindex = location.autoindex;
			resolved.has_redirect = !location.redirect.empty();

			resolved.path = location.root + request_target;
			resolved.index_path = "";

			resolved.get_allowed = location.get_allowed;
			resolved.post_allowed = location.post_allowed;
			resolved.delete_allowed = location.delete_allowed;

			if (resolved.has_redirect)
			{
				resolved.has_redirect = true;
				resolved.path = location.redirect;
				continue;
			}

			if (resolved.is_cgi_directory)
			{
				std::string unsplit_path = resolved.path;
				resolved.script_name = "";

				size_t path_end_index = 0;
				while (path_end_index < unsplit_path.length())
				{
					while (path_end_index < unsplit_path.length()
						&& unsplit_path.at(path_end_index) != '/'
						&& unsplit_path.at(path_end_index) != '?')
					{
						path_end_index++;
					}
					resolved.script_name = unsplit_path.substr(0, path_end_index);

					// TODO: Remove
					// Logger::debug("path_end_index: " + std::to_string(path_end_index));
					// Logger::debug("resolved.script_name: " + resolved.script_name);

					if (!std::filesystem::is_directory(resolved.script_name)
						|| path_end_index >= unsplit_path.length()
						|| unsplit_path.at(path_end_index) == '?')
					{
						break;
					}

					path_end_index++;
				}

				size_t path_len = resolved.script_name.length();
				size_t questionmark_index = unsplit_path.find("?", path_len);
				// TODO: Technically PATH_INFO should also translate stuff like "%2e" to ".", so do we want to do that?
				resolved.path_info = unsplit_path.substr(path_len, questionmark_index - path_len);

				resolved.query_string = unsplit_path.substr(resolved.script_name.length() + resolved.path_info.length());

				// TODO: Remove
				// Logger::debug("unsplit_path: " + unsplit_path);
				// Logger::debug("resolved.script_name: " + resolved.script_name);
				// Logger::debug("resolved.path_info: " + resolved.path_info);
				// Logger::debug("resolved.query_string: " + resolved.query_string);
			}

			if (resolved.has_index)
			{
				resolved.index_path = resolved.path + location.index;
			}
		}
	}

	return resolved;
}

bool Server::_isAllowedMethod(const ResolvedLocation &location, const std::string &method)
{
	if (method == "GET" && !location.get_allowed)
	{
		return false;
	}
	else if (method == "POST" && !location.post_allowed)
	{
		return false;
	}
	else if (method == "DELETE" && !location.delete_allowed)
	{
		return false;
	}

	return true;
}

void Server::_handleClientException(const Client::ClientException &e, Client &client)
{
	Logger::info(std::string("  ") + e.what());

	client.status = e.status;

	_removeClientAttachments(client.client_fd);

	_disableReadingFromClient(client);

	size_t server_index = _getServerIndexFromClientServerName(client);
	const Config::ServerDirective &server = _config.servers.at(server_index);

	client.respondWithError(server.error_pages);

	_enableWritingToClient(client);
}

size_t Server::_getServerIndexFromClientServerName(const Client &client)
{
	const std::vector<size_t> &server_indices = _bind_fd_to_server_indices.at(client.server_fd);

	for (size_t inspected_server_index : server_indices)
	{
		for (const std::string &inspected_server_name : _config.servers.at(inspected_server_index).server_names)
		{
			if (inspected_server_name == client.server_name)
			{
				return inspected_server_index;
			}
		}
	}

	return server_indices.at(0);
}

void Server::_handlePollout(int fd, FdType fd_type, nfds_t pfd_index)
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
	Logger::info(std::string("  Writing from the server to the CGI..."));

	assert(client.cgi_write_state == Client::ServerToCGIState::WRITING);

	size_t max_cgi_write_len = MAX_CGI_WRITE_LEN; // TODO: Read from config
	size_t body_substr_len = std::min(client.body.length() - client.body_index, max_cgi_write_len);

	assert(body_substr_len > 0);

	// TODO: substr() can fail
	std::string body_substr = client.body.substr(client.body_index, body_substr_len);

	client.body_index += body_substr_len;

	Logger::info(std::string("    Sending this body substr to the CGI that has a length of ") + std::to_string(body_substr.length()) + " bytes:\n----------\n" + body_substr + "\n----------\n");

	if (write(client.server_to_cgi_fd, body_substr.c_str(), body_substr.length()) == -1)
	{
		// Happens when the CGI script closed its stdin
		Logger::info(std::string("    write() detected 'Broken pipe'"));

		Logger::info(std::string("    Disabling server_to_cgi POLLOUT"));
		_disableEvent(pfd_index, POLLOUT);

		client.cgi_write_state = Client::ServerToCGIState::DONE;
		_removeClientFd(client.server_to_cgi_fd);

		return;
	}

	// If we don't have anything left to write
	if (client.body_index == client.body.length())
	{
		Logger::info(std::string("    Disabling server_to_cgi POLLOUT"));
		_disableEvent(pfd_index, POLLOUT);

		client.cgi_write_state = Client::ServerToCGIState::DONE;
		_removeClientFd(client.server_to_cgi_fd);
	}
}

void Server::_writeToClient(Client &client, int fd)
{
	Logger::info(std::string("  Writing to the client..."));

	assert(client.client_fd == fd);
	assert(client.client_write_state == Client::ServerToClientState::WRITING);

	size_t max_client_write_len = MAX_CLIENT_WRITE_LEN; // TODO: Read from config; HAS to be >= 1
	size_t response_substr_len = std::min(client.response.length() - client.response_index, max_client_write_len);

	assert(response_substr_len > 0);

	std::string response_substr = client.response.substr(client.response_index, response_substr_len);

	client.response_index += response_substr_len;

	Logger::info(std::string("    Sending this response substr to the client that has a length of ") + std::to_string(response_substr.length()) + " bytes:\n----------\n" + response_substr + "\n----------\n");

	// sleep(5); // TODO: REMOVE

	if (write(fd, response_substr.c_str(), response_substr.length()) == -1)
	{
		// Reached when `curl -v localhost:8080/tests/sent/1m_lines.txt` is cancelled halfway through
		_removeClient(client.client_fd);
		return;
	}

	// sleep(5); // TODO: REMOVE

	if (client.response_index == client.response.length())
	{
		_removeClient(client.client_fd);
	}
}
