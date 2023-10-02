#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

// c++ open.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	open("../foo.txt", O_CREAT | O_WRONLY);

	return EXIT_SUCCESS;
}
