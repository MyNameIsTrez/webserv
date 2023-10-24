#pragma once

#include "Status.hpp"

#include <algorithm>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cctype>

struct LocationDirective
{
	std::string uri;
	bool get_allowed;
	bool post_allowed;
	bool delete_allowed;
	bool autoindex;
	std::string index;
	std::string root;
	std::vector<std::string> cgi_paths; // TODO: Needed?
	std::vector<std::string> cgi_ext; // TODO: Needed?
};

struct ServerDirective
{
	// TODO: Allow autoindex?
	std::string root; // TODO: Make this mandatory
	std::vector<uint16_t> ports;
	std::vector<std::string> server_names;
	size_t client_max_body_size;
	std::string http_redirection;
	std::map<Status::Status, std::string> error_pages;
	std::vector<LocationDirective> locations;
};

class Config
{
public:
	Config(void);
	virtual ~Config(void);

	void init(std::istream &config);
	void printConfigInfo(void); // TODO: Remove

	int connection_queue_length;
	std::string default_file;
	std::vector<ServerDirective> servers;
	std::unordered_map<uint16_t, size_t> port_to_server_index;

	struct ConfigException : public std::runtime_error
	{
		ConfigException(const std::string &message) : std::runtime_error(message) {};
	};

	struct InvalidLineException : public ConfigException
	{
		InvalidLineException() : ConfigException("Error: Invalid line in config") {};
	};

	struct InvalidFileException : public ConfigException
	{
		InvalidFileException() : ConfigException("Error: Unable to open config") {};
	};

	struct EmptyTypeException : public ConfigException
	{
		EmptyTypeException() : ConfigException("Error: Empty type in config") {};
	};

	struct DuplicatePortException : public ConfigException
	{
		DuplicatePortException() : ConfigException("Error: Duplicate port in config") {};
	};

	struct InvalidPortException : public ConfigException
	{
		InvalidPortException() : ConfigException("Error: Invalid port in config") {};
	};

private:
	Config(const Config &src);
	Config &operator=(const Config &src);

	// TODO: Remove all commented out lines
	// int getPort(size_t index);
	// std::string getRoot(void);
	// std::string getIndex_file(void);
	void saveType(std::string type, std::string value);
	void saveConnectionQueueLength(std::string value);
	void saveDefaultFile(std::string value);
	void newServer(std::string line, std::istream &config);
	void saveErrorPages(std::string line, ServerDirective *new_server);
	LocationDirective saveLocation(std::string line, std::istream &config);
	// int checkLine(std::string line);
	void initMetadata(void);
	bool parseBool(const std::string &value);

	// typedef int (*t_jump_function)(std::string line);
};

struct ConfigException : public std::runtime_error
{
	ConfigException(const std::string &message) : std::runtime_error(message){};
};

struct InvalidLineException : public ConfigException
{
	InvalidLineException() : ConfigException("Error: Invalid line in config") {};
};

struct InvalidFileException : public ConfigException
{
	InvalidFileException() : ConfigException("Error: Unable to open config") {};
};

struct EmptyTypeException : public ConfigException
{
	EmptyTypeException() : ConfigException("Error: Empty type in config") {};
};

struct ConflictingServerNameException : public ConfigException
{
	ConflictingServerNameException() : ConfigException("Error: Conflicting server name in config") {};
};

struct InvalidPortException : public ConfigException
{
	InvalidPortException() : ConfigException("Error: Invalid port in config") {};
};
