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

	Config config;
	std::stringstream config_stream(buf);
	try
	{
		config.init(config_stream);
	}
	catch (const ConfigException &a)
	{
	}
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
	while (__extension__ __AFL_LOOP(10000))
#endif
	{
		run(argc, argv, (char *)buf);
	}

	return EXIT_SUCCESS;
}
