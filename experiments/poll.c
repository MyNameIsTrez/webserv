#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void errExit(char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

// TODO: This command does not reach the end of main() because POLLHUP is never received; why is that?
// gcc poll.c -Wall -Wextra -Werror -Wpedantic -Wfatal-errors -g -fsanitize=address,undefined && rm -f myfifo && mkfifo myfifo && echo aaaaabbbbbccccc > myfifo &; ./a.out myfifo
// Code stolen from man example: https://man7.org/linux/man-pages/man2/poll.2.html
int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s file1 file2 file3\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	nfds_t nfds = argc - 1;
	struct pollfd *pfds = calloc(nfds, sizeof(struct pollfd));
	if (pfds == NULL) {
		errExit("malloc");
	}

	// Open each file on command line, and add it to pfds

	for (nfds_t j = 0; j < nfds; j++) {
		pfds[j].fd = open(argv[j + 1], O_RDONLY);
		if (pfds[j].fd == -1) {
			errExit("open");
		}

		printf("Opened \"%s\" on fd %d\n", argv[j + 1], pfds[j].fd);

		pfds[j].events = POLLIN;
	}

	nfds_t num_open_fds = argc - 1;
	char buf[10];

	// Keep calling poll() as long as at least one file descriptor is open
	while (num_open_fds > 0) {
		printf("About to poll()\n");
		int ready = poll(pfds, nfds, -1);
		if (ready == -1) {
			errExit("poll");
		}

		printf("Ready: %d\n", ready);

		// Deal with array returned by poll()
		for (nfds_t j = 0; j < nfds; j++) {
			if (pfds[j].revents != 0) {
				printf("  fd=%d; events: %s%s%s\n", pfds[j].fd,
						(pfds[j].revents & POLLIN)  ? "POLLIN "  : "",
						(pfds[j].revents & POLLHUP) ? "POLLHUP " : "",
						(pfds[j].revents & POLLERR) ? "POLLERR " : "");

				if (pfds[j].revents & POLLIN) {
					ssize_t s = read(pfds[j].fd, buf, sizeof(buf));
					if (s == -1) {
						errExit("read");
					}

					printf("    read %zd bytes: %.*s\n",
							s, (int) s, buf);
				} else { // POLLERR or POLLHUP
					printf("    closing fd %d\n", pfds[j].fd);

					if (close(pfds[j].fd) == -1) {
						errExit("close");
					}

					num_open_fds--;
				}
			}
		}
	}

	printf("All file descriptors closed; bye\n");
	exit(EXIT_SUCCESS);
}
