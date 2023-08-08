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
// Takes an error number like errno and returns its error message string
char *strerror(int errnum);
pid_t fork(void);


// Converts between a short (16 bit) or long (32 bit) value between host and network byte order
uint16_t htons(uint16_t hostshort);
uint32_t htonl(uint32_t hostlong);
uint16_t ntohs(uint16_t netshort);
uint32_t ntohl(uint32_t netlong);


// Waits until some of the passed read, write, or error file descriptors should be handled
// Afterwards one has to loop from 0 to nfds and use FD_ISSET() to know which file descriptors should be handled
// Instead, it's also possible to only iterate over our own vector of the file descriptors we know
int select(int nfds, fd_set *restrict readfds, fd_set *restrict writefds,
           fd_set *restrict errorfds, struct timeval *restrict timeout);


// Like select(), but receives an array of pollfds (use Variable Length Array)
// It can listen to more types of events than just read/write/error
int poll(struct pollfd fds[], nfds_t nfds, int timeout);


// Like poll(), but object-oriented
// The returned file descriptor refers to the instance, and should be passed to epoll_ctl() and epoll_wait()
int epoll_create(int size);

// Registers interest in particular file descriptors
// op is either EPOLL_CTL_ADD (add), EPOLL_CTL_MOD (modify), or EPOLL_CTL_DEL (delete)
// fd needs to both be passed as an argument and inside the event, since the event's fd is a union, so it is optional
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *_Nullable event);

// `events` is filled by this function, allowing us to only loop over the fds that had an event
// `maxevents` allows us to limit the number of events that are returned at once
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);


// Marius recommended *not* using this, since it's macOS-specifc, which means it can't be valgrinded with Docker
int kqueue(void);

// Marius recommended *not* using this, since it's macOS-specifc, which means it can't be valgrinded with Docker
// changelist are the events you want to monitor
// nchanges is the size of changelist (0 if changelist is NULL)
// eventlist are the triggered events, and is filled out by this function
// nevents is the size of eventlist (0 if eventlist is NULL)
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


// Takes a hostname like google.com and returns a list of its IPs
int getaddrinfo(const char *hostname, const char *servname,
                const struct addrinfo *hints, struct addrinfo **res);

void freeaddrinfo(struct addrinfo *ai);

// Takes a getaddrinfo() error code and returns its error message string
const char *gai_strerror(int ecode);


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
