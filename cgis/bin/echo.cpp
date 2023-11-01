#include <cstdlib>
#include <unistd.h>
#include <iostream>

// c++ echo.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined -o echo && echo foo | ./echo bar
int main(int argc, char *argv[], char *envp[])
{
	(void)argc;
	while (*argv) {
		std::cout << *argv << std::endl;
		argv++;
	}
	std::cout << std::endl;
	while (*envp) {
		std::cout << *envp << std::endl;
		envp++;
	}
	std::cout << std::endl;
	for (std::string line; std::getline(std::cin, line);) {
        std::cout << line << std::endl;
    }
}
