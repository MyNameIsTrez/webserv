#pragma once

#include "Client.hpp"
#include "config/Config.hpp"

#include <signal.h>
#include <unordered_set>

class Server
{
  public:
    Server(const Config &config);
    ~Server(void);

    void run(void);

    static volatile sig_atomic_t running;
    static bool shutting_down_gracefully;

  private:
    Server(void);
    Server(Server const &src);
    Server &operator=(Server const &src);

    template <typename T> void _swapRemove(T &vector, size_t index);

    enum class FdType
    {
        SERVER,
        CLIENT,
        SERVER_TO_CGI,
        CGI_TO_SERVER,
    };

    // UTILS
    void _shutDownServers(void);
    void _printContainerSizes(void);
    void _printEvents(const pollfd &pfd, FdType fd_type);
    void _processPfd(size_t pfd_index, std::unordered_set<int> &seen_fds);

    Client &_getClient(int fd);

    void _removeClientFd(int &fd);
    void _removeFd(int &fd);

    void _removeServerToCGIFd(int fd);
    void _removeCGIToServerFd(int fd);

    void _enableEvent(size_t pfd_index, short int event);
    void _disableEvent(size_t pfd_index, short int event);

    void _enableWritingToClient(Client &client);
    void _enableWritingToCGI(Client &client);
    void _disableReadingFromClient(Client &client);

    void _addClientFd(int fd, size_t client_index, FdType fd_type, short int events);
    void _addFd(int fd, FdType fd_type, short int events);

    // POLLERR
    void _handlePollerr(int fd, FdType fd_type);

    // POLLHUP
    void _pollhupCGIToServer(int fd);
    void _cgiEnd(Client &client);

    // POLLIN
    void _handlePollin(int fd, FdType fd_type, bool &skip_client);
    void _acceptClient(int server_fd);
    void _reapChildCGIScripts(void);
    void _readFd(Client &client, int fd, FdType fd_type, bool &skip_client);
    void _removeClient(int fd);
    void _killCGI(int fd);

    void _startCGI(Client &client, const std::string &cgi_execve_path, const std::string &script_name,
                   const std::string &path_info, const std::string &query_string);
    void _execveChild(Client &client, const std::string &cgi_execve_path, const std::string &script_name,
                      const std::string &path_info, const std::string &query_string);
    std::vector<const char *> _getCGIEnv(const std::vector<std::string> &cgi_headers);

    struct ResolvedLocation
    {
        bool resolved;

        bool is_cgi_directory;
        std::string cgi_execve_path;

        std::string path_info;
        std::string query_string;

        bool has_index;
        bool autoindex;
        bool has_redirect;

        std::string path;
        std::string index_path;
        std::string index_directive;

        bool get_allowed;
        bool post_allowed;
        bool delete_allowed;
    };
    ResolvedLocation _resolveToLocation(const std::string &request_target,
                                        const std::vector<Config::LocationDirective> &locations);
    bool _isAllowedMethod(const ResolvedLocation &location, Client::RequestMethod method);

    void _respondClientException(const Client::ClientException &e, Client &client);
    size_t _getServerIndexFromClientServerName(const Client &client);

    // POLLOUT
    void _handlePollout(int fd, FdType fd_type);
    void _writeToCGI(Client &client);
    ssize_t _writeString(int fd, const std::string &msg);
    void _writeToClient(Client &client);

    // SIGNAL HANDLERS
    static void _sigIntHandler(int signum);

    const Config &_config;

    int _unreaped_cgi_count;

    std::unordered_map<int, std::vector<size_t>> _bind_fd_to_server_indices;
    std::unordered_map<int, std::string> _bind_fd_to_port;

    std::unordered_map<pid_t, int> _cgi_pid_to_client_fd;
    std::unordered_map<int, size_t> _fd_to_client_index;
    std::unordered_map<int, size_t> _fd_to_pfd_index;
    std::unordered_map<int, FdType> _fd_to_fd_type;

    std::vector<Client> _clients;
    std::vector<pollfd> _pfds;
};
