#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

// c++ stat.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
int main(void)
{
	// const char *path = "stat.cpp";
	// const char *path = ".";
	// const char *path = "../public/foo/no_perms.html";
	const char *path = "../public/foo/x.html";
	struct stat status;

	if (stat(path, &status) == -1)
	{
		printf("errno: %d\n", errno);
		perror("stat");
		return EXIT_FAILURE;
	}

	printf("st_atim.tv_nsec: %ld\n", status.st_atim.tv_nsec);
	printf("st_atim.tv_sec: %ld\n", status.st_atim.tv_sec);
	printf("st_blksize: %ld\n", status.st_blksize);
	printf("st_blocks: %ld\n", status.st_blocks);
	printf("st_ctim.tv_nsec: %ld\n", status.st_ctim.tv_nsec);
	printf("st_ctim.tv_sec: %ld\n", status.st_ctim.tv_sec);
	printf("st_dev: %lu\n", status.st_dev);
	printf("st_gid: %d\n", status.st_gid);
	printf("st_ino: %lu\n", status.st_ino);
	printf("st_mode: %d\n", status.st_mode);
	printf("S_ISDIR(st_mode): %d\n", S_ISDIR(status.st_mode));
	printf("st_mtim.tv_nsec: %ld\n", status.st_mtim.tv_nsec);
	printf("st_mtim.tv_sec: %ld\n", status.st_mtim.tv_sec);
	printf("st_nlink: %lu\n", status.st_nlink);
	printf("st_rdev: %lu\n", status.st_rdev);
	printf("st_size: %ld\n", status.st_size);
	printf("st_uid: %d\n", status.st_uid);

	return EXIT_SUCCESS;
}
