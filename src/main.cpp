#include "Server.hpp"
#include "config/Config.hpp"
#include "config/JSON.hpp"

#include <filesystem>

struct CouldntOpenConfigFile : public std::runtime_error
{
    CouldntOpenConfigFile() : std::runtime_error("Error: Couldn't open config file")
    {
    }
};

struct ConfigFileTooLarge : public std::runtime_error
{
    ConfigFileTooLarge() : std::runtime_error("Error: Config file too large")
    {
    }
};

int main(int argc, char *argv[])
{
    if (argc != 1 && argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " [configuration_path]" << std::endl;
        return EXIT_FAILURE;
    }

    std::string file_name(argc == 2 ? argv[1] : "webserv.json");

    std::ifstream config_file(file_name);

    if (!config_file.is_open())
    {
        throw CouldntOpenConfigFile();
    }

    if (std::filesystem::file_size(file_name) > 420420)
    {
        throw ConfigFileTooLarge();
    }

    JSON json(config_file, 5);

    Config config(json);

    Server server(config);

    server.run();

    return EXIT_SUCCESS;
}
