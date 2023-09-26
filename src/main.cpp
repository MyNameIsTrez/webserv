#include "Server.hpp"

#include <cstdlib>

int main(void)
{
	// TODO: Error handling
	Server server; // Default configuration path
	// Server server("foo.cfg"); // TODO: Try this
	server.run();
	return EXIT_SUCCESS;
}
