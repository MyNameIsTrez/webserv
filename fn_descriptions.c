#include <arpa/inet.h>
#include <dirent.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>


// Watch this playlist for explanations of most of these functions:
// https://www.youtube.com/playlist?list=PL9IEJIKnBJjH_zM5LnovnoaKlXML5qh17


////////////////////////////////////////////////////////////////////////////////
// List of all allowed functions

// execve, dup, dup2, pipe, strerror, gai_strerror,
// errno, fork, htons, htonl, ntohs, ntohl,
// select, poll, epoll (epoll_create, epoll_ctl,
// epoll_wait), kqueue (kqueue, kevent), socket,
// accept, listen, send, recv, bind, connect,
// getaddrinfo, freeaddrinfo, setsockopt, getsockname,
// getprotobyname, fcntl, close, read, write, waitpid,
// kill, signal, access, stat, opendir, readdir, closedir


////////////////////////////////////////////////////////////////////////////////
// Functions we already know

int execve(const char *path, char *const argv[], char *const envp[]);
int dup(int fildes);
int dup2(int fildes, int fildes2);
int pipe(int fildes[2]);
char *strerror(int errnum);
pid_t fork(void);
int close(int fildes);
ssize_t read(int fildes, void *buf, size_t nbyte);
ssize_t write(int fildes, const void *buf, size_t nbyte);
int access(const char *path, int mode);
pid_t waitpid(pid_t pid, int *stat_loc, int options);


////////////////////////////////////////////////////////////////////////////////
// Byte order functions

// Converts between host and network byte order, either a short (16 bit) or a long (32 bit) integer
uint16_t htons(uint16_t hostshort);
uint32_t htonl(uint32_t hostlong);
uint16_t ntohs(uint16_t netshort);
uint32_t ntohl(uint32_t netlong);


////////////////////////////////////////////////////////////////////////////////
// Event handling functions

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


////////////////////////////////////////////////////////////////////////////////
// Socket functions

// Initializes a socket, and returns a file descriptor that references the socket
// domain can be AF_INET
// type can be SOCK_STREAM
// protocol is 0 (TCP) with streams, and 0 (UDP) for datagrams
int socket(int domain, int type, int protocol);

// Allows setting a port
int bind(int socket, const struct sockaddr *address, socklen_t address_len);

// Allows incoming connections
// `backlog` is the max queue length of pending connections
int listen(int socket, int backlog);

// Waits until an incoming connection from the connection queue can be returned
int accept(int socket, struct sockaddr *restrict address,
           socklen_t *restrict address_len);


////////////////////////////////////////////////////////////////////////////////
// Address info functions

// Takes a hostname like google.com and returns a list of its IPs
// See experiments/getaddrinfo.c
int getaddrinfo(const char *hostname, const char *servname,
                const struct addrinfo *hints, struct addrinfo **res);

void freeaddrinfo(struct addrinfo *ai);

// Takes a getaddrinfo() error code and returns its error message string
const char *gai_strerror(int ecode);


////////////////////////////////////////////////////////////////////////////////
// Directory functions

// These don't recursively walk directories, so you'd have to make your own recursive one
// See the bottom of "man opendir" for a usage example

DIR *opendir(const char *filename);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);


////////////////////////////////////////////////////////////////////////////////
// Socket options

// Further explanations:
// https://stackoverflow.com/a/4233770/13279557
// https://linuxgazette.net/136/pfeiffer.html

// For example, allows setting the maximum receive buffer size, or reusing addresses, etc
int setsockopt(int socket, int level, int option_name,
               const void *option_value, socklen_t option_len);

// For example, allows getting the maximum receive buffer size, or length of timeouts, etc
int getsockname(int socket, struct sockaddr *restrict address,
                socklen_t *restrict address_len);


////////////////////////////////////////////////////////////////////////////////
// Get protocol number and aliases from name

// See experiments/getprotobyname.c
struct protoent *getprotobyname(const char *name);


////////////////////////////////////////////////////////////////////////////////
// File control

// From the Posix standard: https://stackoverflow.com/a/42121676/13279557
// "The dup() function provides an alternative interface to the service provided by fcntl() using the F_DUPFD command.
// The call dup(fildes) shall be equivalent to: fcntl(fildes, F_DUPFD, 0);"
// Where the 0 means that any file descriptor higher than 0 can be returned

// See experiments/fcntl.c
int fcntl(int fildes, int cmd, ...);


////////////////////////////////////////////////////////////////////////////////

int kill(pid_t pid, int sig);

sig_t signal(int sig, sig_t func);

int stat(const char *restrict path, struct stat *restrict buf);


ssize_t send(int socket, const void *buffer, size_t length, int flags);

ssize_t recv(int socket, void *buffer, size_t length, int flags);

int connect(int socket, const struct sockaddr *address, socklen_t address_len);
