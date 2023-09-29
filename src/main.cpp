#include "Server.hpp"

#include <cstdlib>

void check_leaks(void)
{
	system("leaks -q webserv");
}

int main(void)
{
	atexit(check_leaks);

	// TODO: Error handling
	Server server; // Default configuration path
	// Server server("foo.cfg"); // TODO: Try this
	server.run();
	return EXIT_SUCCESS;
}
