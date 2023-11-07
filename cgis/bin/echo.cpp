#include <cstdlib>
#include <unistd.h>
#include <iostream>

// c++ echo.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined -o echo && echo foo | ./echo
int main(int argc, char *argv[], char *envp[])
{
	for (size_t i = 0; i < static_cast<size_t>(argc); i++) {
		std::cout << "argv[" << i << "]: " << argv[i] << std::endl;
	}

	std::cout << std::endl;

	for (size_t i = 0; envp[i]; i++) {
		std::cout << "envp[" << i << "]: " << envp[i] << std::endl;
	}

	std::cout << std::endl;

	size_t i = 0;
	for (std::string line; std::getline(std::cin, line); i++) {
        std::cout << "line[" << i << "]: " << line << std::endl;
    }
}
