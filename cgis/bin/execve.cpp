#include <cstdlib>
#include <unistd.h>

// c++ execve.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined -o execve && debug foo | ./execve debug /foo.txt
int main(int argc, char *argv[], char *envp[])
{
	if (argc != 3)
		return EXIT_FAILURE;

	char *const args[] = {argv[1], argv[2], NULL};
	execve(argv[1], args, envp);

	return EXIT_FAILURE;
}
