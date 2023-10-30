#include "config/Config.hpp"
#include "config/JSON.hpp"
#include "Logger.hpp"
#include "Server.hpp"

#include <cstdlib>

struct FileNoAccess : public std::runtime_error
{
	FileNoAccess() : std::runtime_error("Error: Unable to open config"){};
};

int main(int argc, char *argv[])
{
	if (argc != 1 && argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " [configuration_path]" << std::endl;
		return EXIT_FAILURE;
	}

	Config config;
	try
	{
		std::ifstream config_file(argc == 2 ? argv[1] : "webserv.json");
		if (!config_file.is_open())
			throw FileNoAccess();

		JSON json(config_file);

		config.init(json);
	}
	catch (const Config::ConfigException &e)
	{
		Logger::error(e.what());
		return EXIT_FAILURE;
	}

	Server server(config);
	server.run();

	// signal(SIGCHLD, SIG_DFL); // TODO: Put this back in
	return EXIT_SUCCESS;
}
