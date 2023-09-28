#pragma once

#include "Client.hpp"

namespace FdType
{
	enum FdType
	{
		SERVER,
		SIG_CHLD,
		CLIENT,
		SERVER_TO_CGI,
		CGI_TO_SERVER,
	};
}

class Server
{
public:
	Server(void);
	Server(const std::string &configuration_path);

	virtual ~Server(void);

	void run(void);

private:
	Server(Server const &src);
	Server &operator=(Server const &src);

	template <typename T>
	void _swapRemove(T &vector, size_t index);

	// UTILS
	void _printContainerSizes(void);
	void _printEvents(const pollfd &pfd, FdType::FdType fd_type);
	Client &_getClient(int fd);
	void _removeClientFd(int &fd);
	void _removeFd(int &fd);
	void _enableWritingToClient(Client &client);
	void _addClientFd(int fd, size_t client_index, FdType::FdType fd_type, short int events);
	void _addFd(int fd, FdType::FdType fd_type, short int events);

	// POLLNVAL
	void _handlePollnval(void);

	// POLLERR
	void _handlePollerr(int fd, FdType::FdType fd_type);

	// POLLHUP
	void _pollhupCGIToServer(int fd);

	// POLLIN
	void _handlePollin(int fd, FdType::FdType fd_type, bool &should_continue);
	void _acceptClient();
	void _reapChild(void);
	bool _readFd(Client &client, int fd, FdType::FdType fd_type, bool &should_continue);
	void _removeClient(int fd);
	bool _startCGI(Client &client, int fd, FdType::FdType fd_type);

	// POLLOUT
	void _handlePollout(int fd, FdType::FdType fd_type, nfds_t pfd_index);
	void _writeToCGI(Client &client, nfds_t pfd_index);
	void _writeToClient(Client &client, int fd);

	// SIGCHLD
	static void _sigChldHandler(int signum);

	static int _sig_chld_tube[2];

	int _server_fd;

	std::unordered_map<pid_t, int> _cgi_pid_to_client_fd;
	std::unordered_map<int, size_t> _fd_to_client_index;
	std::unordered_map<int, size_t> _fd_to_pfd_index;
	std::unordered_map<int, FdType::FdType> _fd_to_fd_type;

	std::vector<Client> _clients;
	std::vector<pollfd> _pfds;
};
