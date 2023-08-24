#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define PIPE_READ_INDEX 0
#define PIPE_WRITE_INDEX 1

#define CHILD 0

// gcc cgi.c -Wall -Wextra -Werror -Wpedantic -Wfatal-errors -g && ./a.out "kill.c" "out.txt"
// gcc cgi.c -Wall -Wextra -Werror -Wpedantic -Wfatal-errors -g; echo 'foo\nbar' | ./a.out "kill.c" "out.txt"
// Run this to get zombie counts: ps aux | grep a.out | wc -l
int main(int argc, char *argv[], char *envp[])
{
	(void)argc;
	int pipe_fds_client_to_cgi[2];
	int pipe_fds_cgi_to_client[2];

	while (true)
	{
		if (pipe(pipe_fds_client_to_cgi) == -1)
		{
			perror("pipe");
			exit(EXIT_FAILURE);
		}
		if (pipe(pipe_fds_cgi_to_client) == -1)
		{
			perror("pipe");
			exit(EXIT_FAILURE);
		}

		pid_t forked_pid = fork();
		if (forked_pid == -1)
		{
			perror("fork");
			exit(EXIT_FAILURE);
		}
		else if (forked_pid == CHILD)
		{
			close(pipe_fds_client_to_cgi[PIPE_WRITE_INDEX]);
			close(pipe_fds_cgi_to_client[PIPE_READ_INDEX]);

			if (dup2(pipe_fds_client_to_cgi[PIPE_READ_INDEX], STDIN_FILENO) == -1)
			{
				perror("dup2");
				exit(EXIT_FAILURE);
			}

			if (dup2(pipe_fds_cgi_to_client[PIPE_WRITE_INDEX], STDOUT_FILENO) == -1)
			{
				perror("dup2");
				exit(EXIT_FAILURE);
			}

			// TODO: Define Python path in configuration file?
			const char *path = "/usr/local/bin/python3";
			char *const argv[] = {(char *)"python3", (char *)"print.py", NULL};
			execve(path, argv, envp);

			return EXIT_SUCCESS;
		}
		close(pipe_fds_client_to_cgi[PIPE_READ_INDEX]);
		close(pipe_fds_cgi_to_client[PIPE_WRITE_INDEX]);

		// Client fd
		int in_file_fd = open(argv[1], O_RDONLY);
		if (in_file_fd == -1)
		{
			perror("open");
			exit(EXIT_FAILURE);
		}
		// Client fd
		int out_file_fd = open(argv[2], O_CREAT | O_WRONLY | O_TRUNC, 0644);
		if (out_file_fd == -1)
		{
			perror("open");
			exit(EXIT_FAILURE);
		}

		// TODO: These calls all need to happen separately in the event loop
		{
			// Read some bytes from the client into buffer
			read(in_file_fd, buffer, 12);
			// Send buffer to Python script
			write(pipe_fds_client_to_cgi[PIPE_WRITE_INDEX], buffer, 12);

			// Read Python script stdout into buffer
			read(pipe_fds_cgi_to_client[PIPE_READ_INDEX], buffer, 12);
			// Send buffer to client
			write(out_file_fd, buffer, 12);
		}

		sleep(1);
	}

    return EXIT_SUCCESS;
}
