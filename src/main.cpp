#include "Server.hpp"

#include <cstdlib>
// #include <iostream>

// TODO: Remove this?
// static void sigIntHandler(int signum)
// {
// 	(void)signum;
// 	shutting_down_gracefully = true;
// }

int main(void)
{
	// TODO: Error handling
	Server server; // Default configuration path
	// Server server("foo.cfg"); // TODO: Try this
	server.run();
	return EXIT_SUCCESS;
}
