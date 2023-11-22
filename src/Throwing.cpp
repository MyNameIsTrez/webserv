#include "Throwing.hpp"

#include <fcntl.h>
#include <unistd.h>

namespace T = Throwing;

int T::socket(int domain, int type, int protocol)
{
    int bind_fd = ::socket(domain, type, protocol);
    if (bind_fd == -1)
        throw T::SystemException("socket");
    return bind_fd;
}

void T::setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    if (::setsockopt(sockfd, level, optname, optval, optlen) == -1)
        throw T::SystemException("setsockopt");
}

void T::bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    if (::bind(sockfd, addr, addrlen) == -1)
        throw T::SystemException("bind");
}

void T::listen(int sockfd, int backlog)
{
    if (::listen(sockfd, backlog) == -1)
        throw T::SystemException("listen");
}

void T::signal(int signum, sighandler_t handler)
{
    if (::signal(signum, handler) == SIG_ERR)
        throw T::SystemException("signal");
}

void T::pipe(int pipefd[2])
{
    if (::pipe(pipefd) == -1)
        throw T::SystemException("pipe");
}

void T::close(int fd)
{
    if (::close(fd) == -1)
        throw T::SystemException("close");
}

int T::accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    int fd = ::accept(sockfd, addr, addrlen);
    if (fd == -1)
        throw T::SystemException("accept");
    return fd;
}

int T::fcntl(int fd, int cmd, int arg)
{
    int returned = ::fcntl(fd, cmd, arg);
    if (returned == -1)
        throw T::SystemException("fcntl");
    return returned;
}

ssize_t T::read(int fd, void *buf, size_t count)
{
    ssize_t bytes_read = ::read(fd, buf, count);
    if (bytes_read == -1)
        throw T::SystemException("read");
    return bytes_read;
}

void T::kill(pid_t pid, int sig)
{
    if (::kill(pid, sig) == -1)
        throw T::SystemException("kill");
}

pid_t T::fork(void)
{
    pid_t forked_pid = ::fork();
    if (forked_pid == -1)
        throw T::SystemException("fork");
    return forked_pid;
}

void T::dup2(int oldfd, int newfd)
{
    if (::dup2(oldfd, newfd) == -1)
        throw T::SystemException("dup2");
}

void T::execve(const char *pathname, char *const argv[], char *const envp[])
{
    if (::execve(pathname, argv, envp) == -1)
        throw T::SystemException("execve");
}
