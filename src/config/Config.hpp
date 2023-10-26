#pragma once
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <variant>
#include <vector>
#include <limits>

#include "Status.hpp"
#include "Utils.hpp"
#include "Token.hpp"
#include "JSON.hpp"
#include "Node.hpp"
#include "Tokenizer.hpp"

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
	std::vector<std::string> cgi_ext;	// TODO: Needed?
};

// TODO: Error pages niet vergeten te fixen
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
	Config(const JSON &json);

	std::vector<ServerDirective> servers;

private:
	struct ConfigException : public std::runtime_error
	{
		ConfigException(const std::string &message) : std::runtime_error(message){};
	};
	struct ConfigExceptionUnknownKey : public ConfigException
	{
		ConfigExceptionUnknownKey() : ConfigException("Config exception: Unknown key"){};
	};
};

#endif
