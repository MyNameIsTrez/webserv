#include "Server.hpp"

#include <cstdlib>
// #include <iostream>

// TODO: Remove this?
// static void sigIntHandler(int signum)
// {
// 	(void)signum;
// 	shutting_down_gracefully = true;
// }

// c++ main.cpp Server.cpp Client.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
int main(void)
{
	// TODO: Error handling
	Server server; // Default configuration path
	// Server server("foo.cfg"); // TODO: Try this
	server.run();
	return EXIT_SUCCESS;
}
