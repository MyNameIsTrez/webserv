#pragma once

#include "Client.hpp"
#include "config/Config.hpp"

#include <cstring>

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

struct ResolvedLocation
{
	bool is_cgi_directory;
	CGISettingsDirective cgi_settings;

	bool has_index;
	bool autoindex;
	std::string path;

	bool get_allowed;
	bool post_allowed;
	bool delete_allowed;
};

class Server
{
public:
	Server(const Config &config);
	virtual ~Server(void);

	void run(void);

private:
	Server(void);
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

	void _enableEvent(size_t pfd_index, short int event);
	void _disableEvent(size_t pfd_index, short int event);

	void _enableWritingToClient(Client &client);
	void _enableWritingToCGI(Client &client);
	void _disableReadingFromClient(Client &client);

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
	void _acceptClient(int server_fd);
	void _reapChild(void);
	void _readFd(Client &client, int fd, FdType::FdType fd_type, bool &should_continue);
	void _removeClient(int fd);
	void _removeClientAttachments(int fd);
	void _startCGI(Client &client, const CGISettingsDirective &cgi_settings, const std::string &cgi_execve_argv1);
	std::vector<std::string> _getCGIHeaders(const std::unordered_map<std::string, std::string> &headers);
	void _addMetaVariables(std::vector<std::string> &cgi_headers);
	std::vector<const char *> _getCGIEnv(const std::vector<std::string> &cgi_headers);
	ResolvedLocation _resolveToLocation(const std::string &request_target, const ServerDirective &server);
	bool _isAllowedMethod(const ResolvedLocation &location, const std::string &method);

	// POLLOUT
	void _handlePollout(int fd, FdType::FdType fd_type, nfds_t pfd_index);
	void _writeToCGI(Client &client, nfds_t pfd_index);
	void _writeToClient(Client &client, int fd);

	// SIGNAL HANDLERS
	static void _sigIntHandler(int signum);
	static void _sigChldHandler(int signum);

	struct SystemException : public std::runtime_error
	{
		SystemException(const std::string &function_name)
			: runtime_error("System exception in function '" + function_name + "': " + strerror(errno))
		{
		}
	};

	static int _sig_chld_pipe[2];

	const Config &_config;

	std::unordered_map<int, size_t> _server_fd_to_server_index;

	std::unordered_map<pid_t, int> _cgi_pid_to_client_fd;
	std::unordered_map<int, size_t> _fd_to_client_index;
	std::unordered_map<int, size_t> _fd_to_pfd_index;
	std::unordered_map<int, FdType::FdType> _fd_to_fd_type;

	std::vector<Client> _clients;
	std::vector<pollfd> _pfds;
};
