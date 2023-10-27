#include "Config.hpp"
#include "Logger.hpp"
#include "Server.hpp"

#include <cstdlib>
// #include <signal.h>

int main(int argc, char *argv[])
{
	if (argc != 1 && argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " [configuration_path]" << std::endl;
		return EXIT_FAILURE;
	}

	// TODO: Rewrite
	// {
	// 	// TODO: Change "webserv.conf" to "webserv.json"?
	// 	std::ifstream config_file(argc == 2 ? argv[1] : "webserv.conf");
	// 	if (!config_file.is_open())
	// 		throw Config::InvalidFileException();

	// 	Config config;
	// 	try
	// 	{
	// 		config.init(config_file);
	// 	}
	// 	catch (const Config::ConfigException &e)
	// 	{
	// 		Logger::error(e.what());
	// 		return EXIT_FAILURE;
	// 	}
	// 	// config.printConfigInfo();
	// }

	Config config;
	config.servers = {
		{
			.root=".",
			.locations={
				// {
				// 	.uri="/",
				// 	.get_allowed=true,
				// 	.autoindex=true,
				// },
				{
					.uri="/cgi-bin",
					.get_allowed=true,
					.autoindex=true,
				},
				{
					.uri="/public",
					.get_allowed=true,
					.post_allowed=true,
					.delete_allowed=true,
					.autoindex=true,
				},
				// {
				// 	.uri="/rfcs",
				// 	.get_allowed=true,
				// 	.autoindex=true,
				// }
			}
		}
	};
	config.port_to_server_index.emplace(8080, 0);

	// TODO: Error handling
	Server server(config);
	server.run();
	// signal(SIGCHLD, SIG_DFL); // TODO: Put this back in
	return EXIT_SUCCESS;
}
