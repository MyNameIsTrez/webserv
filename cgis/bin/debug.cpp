#include <cstdlib>
#include <unistd.h>
#include <iostream>

// c++ debug.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined -o debug && debug foo | ./debug
int main(int argc, char *argv[], char *envp[])
{
	std::cout << "Content-Type: text/plain" << std::endl;
	std::cout << "Status: 400 Wow" << std::endl;
	std::cout << std::endl;

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
