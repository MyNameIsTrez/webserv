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
	void printEvents(const pollfd &pfd);
	void removeClient(int fd);
	Client &getClient(int fd);
	void removeFd(int &fd);
	void pollhupServerToCGI();
	void pollhupCGIToServer(int fd);
	void removeServerToCGI(Client &client);
	void pollhupCGIExitDetector(int fd);
	void acceptClient();
	bool startCGI(FdType::FdType fd_type, Client &client, int fd);
	void addCGIFd(int cgi_fd, size_t client_index, FdType::FdType fd_type, short int events);
	void writeServerToCGI(Client &client, nfds_t pfd_index);
	void writeToClient(Client &client, int fd, nfds_t pfd_index);

	bool shutting_down_gracefully; // TODO: Consider removing this

	int server_fd;

	std::unordered_map<pid_t, int> cgi_pid_to_client_fd;
	std::unordered_map<int, size_t> fd_to_client_index;
	std::unordered_map<int, size_t> fd_to_pfd_index;
	std::unordered_map<int, FdType::FdType> fd_to_fd_type;

	std::vector<Client> clients;
	std::vector<pollfd> pfds;
};