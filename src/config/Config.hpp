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
struct Node;

class Config
{
public:
	Config();
	void init(const JSON &json);

	struct CGISettingsDirective
	{
		std::string cgi_execve_path;
		std::string cgi_execve_argv0;
	};

	struct LocationDirective
	{
		std::string uri;

		bool is_cgi_directory;
		CGISettingsDirective cgi_settings;

		bool get_allowed;
		bool post_allowed;
		bool delete_allowed;

		bool autoindex;
		std::string index;
		std::string redirect;

		std::string root;
	};

	struct ListenEntry
	{
		std::string address;
		uint16_t port;
	};

	struct ServerDirective
	{
		std::vector<ListenEntry> listen;

		std::vector<std::string> server_names;
		// std::string http_redirection; // TODO: Willen we dit er weer in zetten?
		std::vector<LocationDirective> locations;
		std::unordered_map<Status::Status, std::string> error_pages;
	};

	std::vector<ServerDirective> servers;
	int connection_queue_length;
	size_t client_max_body_size;

	struct ConfigException : public std::runtime_error
	{
		ConfigException(const std::string &message) : std::runtime_error(message){};
	};

private:
	void _parseConnectionQueueLength(const std::unordered_map<std::string, Node> &root_object);
	void _parseClientMaxBodySize(const std::unordered_map<std::string, Node> &root_object);

	struct ConfigExceptionExpectedConnectionQueueLength : public ConfigException
	{
		ConfigExceptionExpectedConnectionQueueLength() : ConfigException("Config exception: Expected connection_queue_length"){};
	};
	struct ConfigExceptionConnectionQueueLengthIsTooHigh : public ConfigException
	{
		ConfigExceptionConnectionQueueLengthIsTooHigh() : ConfigException("Config exception: connection_queue_length is too high"){};
	};
	struct ConfigExceptionConnectionQueueLengthIsSmallerThanOne : public ConfigException
	{
		ConfigExceptionConnectionQueueLengthIsSmallerThanOne() : ConfigException("Config exception: connection_queue_length is smaller than one"){};
	};

	struct ConfigExceptionExpectedClientMaxBodySize : public ConfigException
	{
		ConfigExceptionExpectedClientMaxBodySize() : ConfigException("Config exception: Expected client_max_body_size"){};
	};

	struct ConfigExceptionExpectedListen : public ConfigException
	{
		ConfigExceptionExpectedListen() : ConfigException("Config exception: Expected listen"){};
	};

	struct ConfigExceptionClientMaxBodySizeIsSmallerThanZero : public ConfigException
	{
		ConfigExceptionClientMaxBodySizeIsSmallerThanZero() : ConfigException("Config exception: client_max_body_size is smaller than zero"){};
	};

	struct ConfigExceptionListenColonIsRequired : public ConfigException
	{
		ConfigExceptionListenColonIsRequired() : ConfigException("Config exception: Listen ':' is required"){};
	};
	struct ConfigExceptionMissingAddressBeforePortColon : public ConfigException
	{
		ConfigExceptionMissingAddressBeforePortColon() : ConfigException("Config exception: Missing address before port ':'"){};
	};
	struct ConfigExceptionNoPortAfterColon : public ConfigException
	{
		ConfigExceptionNoPortAfterColon() : ConfigException("Config exception: No port after ':'"){};
	};
	struct ConfigExceptionInvalidPortValue : public ConfigException
	{
		ConfigExceptionInvalidPortValue() : ConfigException("Config exception: Invalid port value"){};
	};

	struct ConfigExceptionExclusivePropertiesPresent : public ConfigException
	{
		ConfigExceptionExclusivePropertiesPresent() : ConfigException("Config exception: Exclusive properties present"){};
	};

	struct ConfigExceptionMissingCGISettingsProperty : public ConfigException
	{
		ConfigExceptionMissingCGISettingsProperty() : ConfigException("Config exception: Missing cgi_settings property"){};
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
