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
	std::string server_name;
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

	// int get_port(size_t index);
	// std::string get_root(void);
	// std::string get_index_file(void);
	void new_server(std::string line, std::istream &config);
	void save_error_pages(std::string line, ServerData *new_server);
	PageData save_page(std::string line, std::istream &config);
	// int check_line(std::string line);
	// void print_server_info(size_t index);

	// typedef int (*t_jump_function)(std::string line);

	size_t _max_connections;
	std::string _default_file;
	std::vector<ServerData> servers;

private:
	Config(const Config &src);
	Config &operator=(const Config &src);
};

struct ConfigException : public std::runtime_error
{
	ConfigException(const std::string &message) : std::runtime_error(message) {};
};

struct InvalidLineException : public ConfigException
{
	InvalidLineException() : ConfigException("Error: Invalid line in config file") {};
};

struct InvalidFileException : public ConfigException
{
	InvalidFileException() : ConfigException("Error: Unable to open config file") {};
};

struct EmptyTypeException : public ConfigException
{
	EmptyTypeException() : ConfigException("Error: Empty type in config file") {};
};
