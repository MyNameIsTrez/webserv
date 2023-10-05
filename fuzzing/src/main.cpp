#include "../../src/Config.hpp"

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#if defined AFL || defined GCOV
__AFL_FUZZ_INIT()
#endif

static void run(int argc, char *argv[], char *buf)
{
	(void)argc;
	(void)argv;

	// So x.txt doesn't take the same path as an empty file,
	// which'd result in "WARNING: Down to zero bytes"
	// if (buf[0] == 'x')
	// {
	// 	exit(EXIT_FAILURE);
	// }

	// assert(argv[0] != NULL);

	Config config;
	std::stringstream config_stream(buf);
	try
	{
		config.init(config_stream);
	}
	catch (const ConfigException &a)
	{
	}

	// if (argv[1])
	// {
	// 	int fd = open("path.txt", O_RDONLY | O_CREAT);
	// 	if (fd == -1) abort();
	// 	if (write(fd, argv[1], strlen(argv[1])) == -1) abort();
	// 	close(fd);
	// 	// char read_buf[1];
	// 	// ssize_t bytes_read = read(fd, read_buf, 1);
	// 	// close(fd);
	// 	// if (bytes_read == -1) abort();

	// 	// if (read_buf[0] == 'm') abort();
	// }

	// if (buf[0] == 'y')
	// {
	// 	throw std::runtime_error("foo");
	// }
	// if (buf[0] == 'z')
	// {
	// 	abort();
	// }
	// // Infinite loop
	// if (buf[0] == 'p')
	// {
	// 	while (1)
	// 		printf("a");
	// 	abort(); // Demonstrates not being reached
	// }

	// if (buf[0] == 'a')
	// {
	// 	if (buf[1] == 'b')
	// 	{
	// 		if (buf[2] == 'c')
	// 		{
	// 			abort();
	// 			// if (buf[3] == 'd')
	// 			// {
	// 			// 	if (buf[4] == 'e')
	// 			// 	{
	// 			// 	}
	// 			// }
	// 		}
	// 	}
	// }
}

int main(int argc, char *argv[])
{
#ifdef __AFL_HAVE_MANUAL_CONTROL
  __AFL_INIT();
#endif

	printf("Started program\n");

	unsigned char *buf = (unsigned char *)"";
#ifdef AFL
	argc = 2;
	buf = __AFL_FUZZ_TESTCASE_BUF;
#elif defined GCOV || defined CTMIN
	argc = 2;
	buf = (unsigned char *)calloc(1024 + 1, sizeof(char));
	if (read(0, buf, 1024) == -1)
	{
		perror("read()");
		return EXIT_FAILURE;
	}
#endif

#if defined AFL || defined GCOV
	// __extension__ is necessary when using -Wpedantic
	while (__extension__ __AFL_LOOP(1000))
#endif
	{
		run(argc, argv, (char *)buf);
	}

	return EXIT_SUCCESS;
}
