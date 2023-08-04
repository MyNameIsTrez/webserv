// execve, dup, dup2, pipe, strerror, gai_strerror,
// errno, fork, htons, htonl, ntohs, ntohl,
// select, poll, epoll (epoll_create, epoll_ctl,
// epoll_wait), kqueue (kqueue, kevent), socket,
// accept, listen, send, recv, bind, connect,
// getaddrinfo, freeaddrinfo, setsockopt, getsockname,
// getprotobyname, fcntl, close, read, write, waitpid,
// kill, signal, access, stat, opendir, readdir, closedir

#include <arpa/inet.h>
#include <dirent.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>

int execve(const char *path, char *const argv[], char *const envp[]);
int dup(int fildes);
int dup2(int fildes, int fildes2);
int pipe(int fildes[2]);
char *strerror(int errnum);
const char *gai_strerror(int ecode);
pid_t fork(void);
uint16_t htons(uint16_t hostshort);
uint32_t htonl(uint32_t hostlong);
uint16_t ntohs(uint16_t netshort);
uint32_t ntohl(uint32_t netlong);
int select(int nfds, fd_set *restrict readfds, fd_set *restrict writefds,
           fd_set *restrict errorfds, struct timeval *restrict timeout);
int poll(struct pollfd fds[], nfds_t nfds, int timeout);
int epoll_create(int size);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *_Nullable event);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
int kqueue(void);
int kevent(int kq, const struct kevent *changelist, int nchanges,
           struct kevent *eventlist, int nevents, const struct timespec *timeout);
int socket(int domain, int type, int protocol);
int accept(int socket, struct sockaddr *restrict address,
           socklen_t *restrict address_len);
int listen(int socket, int backlog);
ssize_t send(int socket, const void *buffer, size_t length, int flags);
ssize_t recv(int socket, void *buffer, size_t length, int flags);
int bind(int socket, const struct sockaddr *address, socklen_t address_len);
int connect(int socket, const struct sockaddr *address, socklen_t address_len);
int getaddrinfo(const char *hostname, const char *servname,
                const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *ai);
int setsockopt(int socket, int level, int option_name,
               const void *option_value, socklen_t option_len);
int getsockname(int socket, struct sockaddr *restrict address,
                socklen_t *restrict address_len);
struct protoent *getprotobyname(const char *name);
int fcntl(int fildes, int cmd, ...);
int close(int fildes);
ssize_t read(int fildes, void *buf, size_t nbyte);
ssize_t write(int fildes, const void *buf, size_t nbyte);
pid_t waitpid(pid_t pid, int *stat_loc, int options);
int kill(pid_t pid, int sig);
sig_t signal(int sig, sig_t func);
int access(const char *path, int mode);
int stat(const char *restrict path, struct stat *restrict buf);
DIR *opendir(const char *filename);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
