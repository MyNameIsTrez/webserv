#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

// c++ fcntl.cpp -Wall -Wextra -Werror -Wpedantic -Wfatal-errors -g && ./a.out 2 && ./a.out 3
int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Usage: %s <descriptor>\n", argv[0]);
		exit(1);
	}

	int fd = atoi(argv[1]);
	int val = fcntl(fd, F_GETFL, 0);

	switch (val & O_ACCMODE)
	{
	case O_RDONLY:
		printf("Read-only");
		break;
	case O_WRONLY:
		printf("Write-only");
		break;
	case O_RDWR:
		printf("Read/Write");
		break;
	default:
		printf("Unknown access mode");
	}

	if (val & O_APPEND)
	{
		printf(", Append");
	}
	if (val & O_NONBLOCK)
	{
		printf(", Non-blocking");
	}
	if (val & O_SYNC)
	{
		printf(", Synchronous writes");
	}

	printf("\n");

	return EXIT_SUCCESS;
}
