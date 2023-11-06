#include <cstdlib>
#include <unistd.h>

// c++ execve.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined -o execve && echo foo | ./execve echo
int main(int argc, char *argv[], char *envp[])
{
	if (argc != 2)
		return EXIT_FAILURE;
	char * const args[] = {argv[1], NULL};
	execve(argv[1], args, envp);
}
