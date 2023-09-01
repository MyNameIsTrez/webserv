#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>

#define PIPE_READ_INDEX 0
#define PIPE_WRITE_INDEX 1

#define CHILD 0

#define MAX_RECEIVED_LEN 30
#define MAX_SENT_LEN 40

// c++ cgi.cpp -Wall -Wextra -Werror -Wpedantic -Wfatal-errors -g && ./a.out "server.cpp" "out.txt"
// Run this to get zombie counts: ps aux | wc -l
int main(int argc, char *argv[], char *envp[])
{
	(void)argc;

	int tube_client_to_cgi[2];
	int tube_cgi_to_client[2];

	while (true)
	{
		fprintf(stderr, "Starting loop\n");

		if (pipe(tube_client_to_cgi) == -1)
		{
			perror("pipe");
			return EXIT_FAILURE;
		}
		if (pipe(tube_cgi_to_client) == -1)
		{
			perror("pipe");
			return EXIT_FAILURE;
		}

		pid_t forked_pid = fork();
		if (forked_pid == -1)
		{
			perror("fork");
			return EXIT_FAILURE;
		}
		else if (forked_pid == CHILD)
		{
			close(tube_client_to_cgi[PIPE_WRITE_INDEX]);
			close(tube_cgi_to_client[PIPE_READ_INDEX]);

			if (dup2(tube_client_to_cgi[PIPE_READ_INDEX], STDIN_FILENO) == -1)
			{
				perror("dup2");
				return EXIT_FAILURE;
			}
			close(tube_client_to_cgi[PIPE_READ_INDEX]);

			if (dup2(tube_cgi_to_client[PIPE_WRITE_INDEX], STDOUT_FILENO) == -1)
			{
				perror("dup2");
				return EXIT_FAILURE;
			}
			close(tube_cgi_to_client[PIPE_WRITE_INDEX]);

			fprintf(stderr, "Child is going to exec Python\n");
			// TODO: Define Python path in configuration file?
			const char *path = "/usr/local/bin/python3";
			char *const argv[] = {(char *)"python3", (char *)"print.py", NULL};
			execve(path, argv, envp);

			perror("execve");
			return EXIT_FAILURE;
		}

		close(tube_client_to_cgi[PIPE_READ_INDEX]);
		close(tube_cgi_to_client[PIPE_WRITE_INDEX]);

		// In server.c this is just client_fd
		int in_file_fd = open(argv[1], O_RDONLY);
		if (in_file_fd == -1)
		{
			perror("open");
			return EXIT_FAILURE;
		}
		// In server.c this is just client_fd
		int out_file_fd = open(argv[2], O_CREAT | O_WRONLY | O_TRUNC, 0644);
		if (out_file_fd == -1)
		{
			perror("open");
			return EXIT_FAILURE;
		}

		// TODO: These read() and write() calls all need to happen separately in the event loop
		{
			char received[MAX_RECEIVED_LEN + 1];
			bzero(received, MAX_RECEIVED_LEN + 1);

			// Read from the client into 'received'
			fprintf(stderr, "Parent is going to read from client\n");
			if (read(in_file_fd, received, MAX_RECEIVED_LEN) == -1)
			{
				perror("read");
				return EXIT_FAILURE;
			}

			// Write 'received' to Python script stdin
			fprintf(stderr, "Parent is going to write '%s' to Python\n", received);
			if (write(tube_client_to_cgi[PIPE_WRITE_INDEX], received, MAX_RECEIVED_LEN) == -1)
			{
				perror("write");
				return EXIT_FAILURE;
			}
			close(tube_client_to_cgi[PIPE_WRITE_INDEX]);
		}

		{
			char sent[MAX_SENT_LEN + 1];
			bzero(sent, MAX_SENT_LEN + 1);

			// Read Python script stdout into 'sent'
			fprintf(stderr, "Parent is going to read from Python\n");
			if (read(tube_cgi_to_client[PIPE_READ_INDEX], sent, MAX_SENT_LEN) == -1)
			{
				perror("read");
				return EXIT_FAILURE;
			}
			close(tube_cgi_to_client[PIPE_READ_INDEX]);

			fprintf(stderr, "Parent is going to send '%s' to client\n", sent);
			// TODO: Don't use strlen
			if (write(out_file_fd, sent, strlen(sent)) == -1)
			{
				perror("write");
				return EXIT_FAILURE;
			}
		}

		// TODO: Don't waitpid(), but do something else
		int child_exit_status;
		waitpid(forked_pid, &child_exit_status, 0);
		// TODO: Why is the child_exit_status seemingly shifted left by 8 bits?
		fprintf(stderr, "Child PID exit status was %d\n\n", child_exit_status);

		close(in_file_fd);
		close(out_file_fd);

		sleep(1);
	}

	return EXIT_SUCCESS;
}
