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

struct PageData // TODO: Rename to LocationData?
{
	std::string page_path;
	std::vector<std::string> allowed_methods;
	bool autoindex;
	std::string index_file;
	std::string root;
	std::vector<std::string> cgi_paths;
	std::vector<std::string> cgi_ext;
};

struct ServerData
{
	std::vector<std::string> ports;
	std::vector<std::string> server_names;
	std::string root_path;
	std::string index_file;
	size_t client_max_body_size;
	std::string http_redirection;
	std::map<Status::Status, std::string> error_pages; // TODO: Use error enum as key
	std::vector<PageData> page_data; // TODO: Turn into <page_path, PageData> map?
};

class Config
{
public:
	Config(void);
	virtual ~Config(void);

	void init(std::istream &config);

	size_t max_connections;
	std::string default_file;
	std::vector<ServerData> servers;

	std::unordered_map<std::string, size_t> http_host_header_to_server_index;
	std::unordered_map<std::string, size_t> port_to_default_server_index;
	std::unordered_set<uint16_t> port_numbers;

private:
	Config(const Config &src);
	Config &operator=(const Config &src);

	// TODO: Remove all commented out lines
	// int getPort(size_t index);
	// std::string getRoot(void);
	// std::string getIndex_file(void);
	void newServer(std::string line, std::istream &config);
	void saveErrorPages(std::string line, ServerData *new_server);
	PageData savePage(std::string line, std::istream &config);
	// int checkLine(std::string line);
	// void printServerInfo(size_t index);
	void initMetadata(void);

	// typedef int (*t_jump_function)(std::string line);
};

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

struct ConflictingServerNameException : public ConfigException
{
	ConflictingServerNameException() : ConfigException("Error: Conflicting server name in config") {};
};

struct InvalidPortException : public ConfigException
{
	InvalidPortException() : ConfigException("Error: Invalid port in config") {};
};
