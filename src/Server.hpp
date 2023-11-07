#pragma once

#include "Client.hpp"
#include "config/Config.hpp"

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

	enum class FdType
	{
		SERVER,
		SIG_CHLD,
		CLIENT,
		SERVER_TO_CGI,
		CGI_TO_SERVER,
	};

	// UTILS
	void _printContainerSizes(void);
	void _printEvents(const pollfd &pfd, FdType fd_type);

	Client &_getClient(int fd);

	void _removeClientFd(int &fd);
	void _removeFd(int &fd);

	void _enableEvent(size_t pfd_index, short int event);
	void _disableEvent(size_t pfd_index, short int event);

	void _enableWritingToClient(Client &client);
	void _enableWritingToCGI(Client &client);
	void _disableReadingFromClient(Client &client);

	void _addClientFd(int fd, size_t client_index, FdType fd_type, short int events);
	void _addFd(int fd, FdType fd_type, short int events);

	// POLLNVAL
	void _handlePollnval(void);

	// POLLERR
	void _handlePollerr(int fd, FdType fd_type);

	// POLLHUP
	void _pollhupCGIToServer(int fd);

	// POLLIN
	void _handlePollin(int fd, FdType fd_type, bool &skip_client);
	void _acceptClient(int server_fd);
	void _reapChild(void);
	void _readFd(Client &client, int fd, FdType fd_type, bool &skip_client);
	void _removeClient(int fd);
	void _removeClientAttachments(int fd);
	void _startCGI(Client &client, const Config::CGISettingsDirective &cgi_settings, const std::string &location_path);
	std::string _getCGIExecveArgv1(const std::string &location_path);
	std::vector<std::string> _getCGIHeaders(const std::unordered_map<std::string, std::string> &headers);
	void _addMetaVariables(std::vector<std::string> &cgi_headers);
	std::vector<const char *> _getCGIEnv(const std::vector<std::string> &cgi_headers);

	struct ResolvedLocation
	{
		bool resolved;

		bool is_cgi_directory;
		Config::CGISettingsDirective cgi_settings;

		bool has_index;
		bool autoindex;
		bool has_redirect;

		std::string path;

		bool get_allowed;
		bool post_allowed;
		bool delete_allowed;
	};
	ResolvedLocation _resolveToLocation(const std::string &request_target, const Config::ServerDirective &server);
	bool _isAllowedMethod(const ResolvedLocation &location, const std::string &method);

	void _handleClientException(const Client::ClientException &e, Client &client);
	size_t _getServerIndexFromClientServerName(const Client &client);

	// POLLOUT
	void _handlePollout(int fd, FdType fd_type, nfds_t pfd_index);
	void _writeToCGI(Client &client, nfds_t pfd_index);
	void _writeToClient(Client &client, int fd);

	// SIGNAL HANDLERS
	static void _sigIntHandler(int signum);
	static void _sigChldHandler(int signum);

	static int _sig_chld_pipe[2];

	const Config &_config;

	// If the Host server name doesn't match any server directive's server names,
	// default to the first server index (Note that the port in the "Host" header is always entirely ignored)
	// TODO: Default construct
	std::unordered_map<int, std::vector<size_t>> _bind_fd_to_server_indices;
	std::unordered_map<int, std::string> _bind_fd_to_port;

	std::unordered_map<pid_t, int> _cgi_pid_to_client_fd;
	std::unordered_map<int, size_t> _fd_to_client_index;
	std::unordered_map<int, size_t> _fd_to_pfd_index;
	std::unordered_map<int, FdType> _fd_to_fd_type;

	std::vector<Client> _clients;
	std::vector<pollfd> _pfds;
};
