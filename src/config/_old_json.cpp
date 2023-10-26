#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <variant>
#include <vector>
#include <limits>

struct ServerDirective
{
	std::vector<uint16_t> ports;
};

class Server
{
public:
	Server(const Config &config) : _config(config) {}

	void run()
	{
		for (const auto &server : _config.servers)
		{
			for (const uint16_t &port : server.ports)
			{
				std::cout << "port: '" << port << "'" << std::endl;
			}
		}
	}

private:
	const Config &_config;
};

// clear && c++ json.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wswitch -Wimplicit-fallthrough -Wfatal-errors -g -fsanitize=address,undefined -std=c++2b && ./a.out
int main()
{
	std::ifstream file("webserv.json");

	// TODO:
	// if (!file.is_open()) throw ??;

	JSON json(file);

	Config config(json);

	Server server(config);
	server.run();
}
