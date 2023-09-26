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
	void swapRemove(T &vector, size_t index);

	// UTILS
	void printContainerSizes(void);
	void printEvents(const pollfd &pfd, FdType::FdType fd_type);
	Client &getClient(int fd);
	void removeFd(int &fd);
	void enableWritingToClient(Client &client);
	void addClientFd(int fd, size_t client_index, FdType::FdType fd_type, short int events);
	void addFd(int fd, FdType::FdType fd_type, short int events);

	// POLLNVAL
	void handlePollnval(void);

	// POLLERR
	void handlePollerr(int fd, FdType::FdType fd_type);

	// POLLHUP
	void handlePollhup(int fd, FdType::FdType fd_type, nfds_t pfd_index, bool &should_continue);
	void pollhupCGIToServer(int fd);
	void reapChild(void);

	// POLLIN
	void handlePollin(int fd, FdType::FdType fd_type, bool &should_continue);
	void acceptClient();
	bool readFd(Client &client, int fd, FdType::FdType fd_type, bool &should_continue);
	void removeClient(int fd);
	bool startCGI(Client &client, int fd, FdType::FdType fd_type);

	// POLLOUT
	void handlePollout(int fd, FdType::FdType fd_type, nfds_t pfd_index);
	void writeToCGI(Client &client, nfds_t pfd_index);
	void writeToClient(Client &client, int fd, nfds_t pfd_index);

	// SIGCHLD
	static void sigChldHandler(int signum);

	static int _sig_chld_tube[2];

	int server_fd;

	std::unordered_map<pid_t, int> cgi_pid_to_client_fd;
	std::unordered_map<int, size_t> fd_to_client_index;
	std::unordered_map<int, size_t> fd_to_pfd_index;
	std::unordered_map<int, FdType::FdType> fd_to_fd_type;

	std::vector<Client> clients;
	std::vector<pollfd> pfds;
};
