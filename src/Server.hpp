#pragma once

#include "Client.hpp"

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

	// TODO: Enact some sort of ordering both in these declarations and the definitions
	void printEvents(const pollfd &pfd, FdType::FdType fd_type);
	void handlePollnval(void);
	void handlePollerr(int fd);
	void handlePollhup(int fd, FdType::FdType fd_type, nfds_t pfd_index, bool &should_continue);
	void handlePollin(int fd, FdType::FdType fd_type, bool &should_continue);
	void handlePollout(int fd, FdType::FdType fd_type, nfds_t pfd_index);
	void removeClient(int fd);
	Client &getClient(int fd);
	void removeFd(int &fd);
	void pollhupCGIToServer(int fd);
	void pollhupCGIExitDetector(int fd);
	void acceptClient();
	bool readFd(Client &client, int fd, FdType::FdType fd_type, bool &should_continue);
	bool startCGI(Client &client, int fd, FdType::FdType fd_type);
	void addCGIFd(int cgi_fd, size_t client_index, FdType::FdType fd_type, short int events);
	void writeServerToCGI(Client &client, nfds_t pfd_index);
	void writeToClient(Client &client, int fd, nfds_t pfd_index);

	int server_fd;

	std::unordered_map<pid_t, int> cgi_pid_to_client_fd;
	std::unordered_map<int, size_t> fd_to_client_index;
	std::unordered_map<int, size_t> fd_to_pfd_index;
	std::unordered_map<int, FdType::FdType> fd_to_fd_type;

	std::vector<Client> clients;
	std::vector<pollfd> pfds;
};
