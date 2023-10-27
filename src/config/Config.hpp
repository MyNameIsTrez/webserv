#pragma once

#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <variant>
#include <vector>
#include <limits>
#include <unordered_map>

#include "../Status.hpp"

class JSON;

struct LocationDirective
{
	std::string uri;
	bool get_allowed;
	bool post_allowed;
	bool delete_allowed;
	bool autoindex;
	std::string index;
	std::string root;
};

struct ServerDirective
{
	std::vector<uint16_t> ports;
	std::vector<std::string> server_names;
	size_t client_max_body_size;
	// std::string http_redirection; // TODO: Willen we dit er weer in zetten?
	std::map<Status::Status, std::string> error_pages;
	std::vector<LocationDirective> locations;
	size_t connection_queue_length;
};

class Config
{
public:
	Config();
	void init(const JSON &json);

	std::vector<ServerDirective> servers;
	std::unordered_map<uint16_t, size_t> port_to_server_index;

	struct ConfigException : public std::runtime_error
	{
		ConfigException(const std::string &message) : std::runtime_error(message){};
	};

private:
	struct ConfigExceptionUnknownKey : public ConfigException
	{
		ConfigExceptionUnknownKey() : ConfigException("Config exception: Unknown key"){};
	};
	struct ConfigExceptionServerExpectedConnectionQueueLength : public ConfigException
	{
		ConfigExceptionServerExpectedConnectionQueueLength() : ConfigException("Config exception: Server expected connection_queue_length"){};
	};
	struct ConfigExceptionServerExpectedClientMaxBodySize : public ConfigException
	{
		ConfigExceptionServerExpectedClientMaxBodySize() : ConfigException("Config exception: Server expected client_max_body_size"){};
	};
	struct ConfigExceptionServerExpectedListen : public ConfigException
	{
		ConfigExceptionServerExpectedListen() : ConfigException("Config exception: Server expected listen"){};
	};
	struct ConfigExceptionServerInvalidErrorPageCode : public ConfigException
	{
		ConfigExceptionServerInvalidErrorPageCode() : ConfigException("Config exception: Server invalid error page code"){};
	};
	struct ConfigExceptionServerDuplicatePort : public ConfigException
	{
		ConfigExceptionServerDuplicatePort() : ConfigException("Config exception: Server duplicate port"){};
	};
};
