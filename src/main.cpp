#include "Config.hpp"
#include "Server.hpp"

#include <cstdlib>
// #include <signal.h>

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		std::cout << "Expected exactly one configuration path argument" << std::endl;
		return EXIT_FAILURE;
	}

	// TODO: Default configuration if argc<=1?

	Config config;
	try
	{
		config.init(argv[1]);
	}
	catch (const InvalidLineException &a)
	{
		std::cout << a.what() << std::endl;
		return EXIT_FAILURE;
	}
	config.print_config_info();

	// TODO: Error handling
	// Server server(config);
	// server.run();
	// signal(SIGCHLD, SIG_DFL); // TODO: Put this back in
	return EXIT_SUCCESS;
}
