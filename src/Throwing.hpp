#pragma once

#include <csignal>
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>

namespace Throwing
{
struct SystemException : public std::runtime_error
{
    SystemException(const std::string &function_name)
        : runtime_error("System exception in function '" + function_name + "': " + strerror(errno))
    {
    }
    SystemException(const std::string &function_name, const std::string &error_message)
        : runtime_error("System exception in function '" + function_name + "': " + error_message)
    {
    }
};

int socket(int domain, int type, int protocol);
void setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
void bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
void listen(int sockfd, int backlog);
void signal(int signum, sighandler_t handler);
void pipe(int pipefd[2]);
void close(int fd);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
ssize_t read(int fd, void *buf, size_t count);
pid_t waitpid(pid_t pid, int *wstatus, int options);
void kill(pid_t pid, int sig);
pid_t fork(void);
void dup2(int oldfd, int newfd);
void execve(const char *pathname, char *const argv[], char *const envp[]);
}; // namespace Throwing
