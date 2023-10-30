#pragma once

#include <cassert>
#include <fstream>
#include <iostream>
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
	size_t connection_queue_length;
	size_t client_max_body_size;
	std::vector<uint16_t> ports;

	std::vector<std::string> server_names;
	// std::string http_redirection; // TODO: Willen we dit er weer in zetten?
	std::vector<LocationDirective> locations;
	std::unordered_map<Status::Status, std::string> error_pages;
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
	struct ConfigExceptionExpectedConnectionQueueLength : public ConfigException
	{
		ConfigExceptionExpectedConnectionQueueLength() : ConfigException("Config exception: Expected connection_queue_length"){};
	};
	struct ConfigExceptionExpectedClientMaxBodySize : public ConfigException
	{
		ConfigExceptionExpectedClientMaxBodySize() : ConfigException("Config exception: Expected client_max_body_size"){};
	};
	struct ConfigExceptionExpectedListen : public ConfigException
	{
		ConfigExceptionExpectedListen() : ConfigException("Config exception: Expected listen"){};
	};

	struct ConfigExceptionPortIsHigherThan65535 : public ConfigException
	{
		ConfigExceptionPortIsHigherThan65535() : ConfigException("Config exception: Port is higher than 65535"){};
	};
	struct ConfigExceptionDuplicatePort : public ConfigException
	{
		ConfigExceptionDuplicatePort() : ConfigException("Config exception: Duplicate port"){};
	};

	struct ConfigExceptionClientMaxBodySizeIsSmallerThanZero : public ConfigException
	{
		ConfigExceptionClientMaxBodySizeIsSmallerThanZero() : ConfigException("Config exception: client_max_body_size is smaller than zero"){};
	};
	struct ConfigExceptionConnectionQueueLengthIsSmallerThanOne : public ConfigException
	{
		ConfigExceptionConnectionQueueLengthIsSmallerThanOne() : ConfigException("Config exception: connection_queue_length is smaller than one"){};
	};

	struct ConfigExceptionBothAutoindexAndIndex : public ConfigException
	{
		ConfigExceptionBothAutoindexAndIndex() : ConfigException("Config exception: Both autoindex and index"){};
	};

	struct ConfigExceptionInvalidErrorPageCode : public ConfigException
	{
		ConfigExceptionInvalidErrorPageCode() : ConfigException("Config exception: Invalid error page code"){};
	};

	struct ConfigExceptionUnknownKey : public ConfigException
	{
		ConfigExceptionUnknownKey() : ConfigException("Config exception: Unknown key"){};
	};
};
