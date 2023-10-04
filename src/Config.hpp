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
	std::vector<int> ports;
	std::string server_name;
	std::string root_path;
	std::string index_file;
	size_t client_max_body_size;
	std::string http_redirection;
	std::map<Status::Status, std::string> error_pages; // TODO: Use error enum as key
	std::vector<PageData> page_data;				   // TODO: Turn into <page_path, PageData> map?
};

class Config
{
public:
	Config(void);
	virtual ~Config(void);

	void init(std::string file);

	void save_type(std::string line, std::string type);
	// int get_port(size_t index);
	// std::string get_root(void);
	// std::string get_index_file(void);
	void save_max_connections(std::string line);
	void save_default_file(std::string line);
	void new_server(std::string line, std::ifstream &config);
	void save_error_pages(std::string line, ServerData *new_server);
	PageData save_page(std::string line, std::ifstream &config);
	// int check_line(std::string line);
	// void print_server_info(size_t index);

	// typedef int (*t_jump_function)(std::string line);

	void print_config_info(void);

	size_t _max_connections;
	std::string _default_file;
	std::vector<ServerData> serverdata;

private:
	Config(const Config &src);
	Config &operator=(const Config &src);
};

struct InvalidLineException : public std::runtime_error
{
	InvalidLineException() : runtime_error("Error: Invalid line in config file"){};
};

struct InvalidFileException : public std::runtime_error
{
	InvalidFileException() : runtime_error("Error: Unable to open config file"){};
};

struct EmptyTypeException : public std::runtime_error
{
	EmptyTypeException() : runtime_error("Error: Empty type in config file"){};
};