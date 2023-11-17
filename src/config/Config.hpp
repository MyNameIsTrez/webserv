#pragma once

#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <netdb.h>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "../Status.hpp"

class JSON;
struct Node;

class Config
{
public:
	Config();
	void init(const JSON &json);

	struct LocationDirective
	{
		std::string uri;

		std::string cgi_execve_path;

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
		std::string port;
	};

	struct ServerDirective
	{
		std::vector<ListenEntry> listen;

		std::vector<std::string> server_names;
		std::vector<LocationDirective> locations;
		std::unordered_map<Status::Status, std::string> error_pages;
	};

	struct BindInfo
	{
		in_addr_t s_addr;
		in_port_t sin_port;

		inline bool operator<(const BindInfo &rhs) const
		{
			return std::tie(this->s_addr, this->sin_port) < std::tie(rhs.s_addr, rhs.sin_port);
		}
	};

	int connection_queue_length;
	size_t client_max_body_size;
	std::vector<ServerDirective> servers;
	std::map<BindInfo, std::vector<size_t>> bind_info_to_server_indices;

	struct ConfigException : public std::runtime_error
	{
		ConfigException(const std::string &message) : std::runtime_error(message){};
	};

private:
	void _parseConnectionQueueLength(const std::unordered_map<std::string, Node> &root_object);
	void _parseClientMaxBodySize(const std::unordered_map<std::string, Node> &root_object);

	ListenEntry _parseListen(const Node &listen_node);
	LocationDirective _parseLocation(const std::pair<std::string, Node> &location_node);

	void _fillBindInfoToServerIndices();

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
	struct ConfigExceptionExpectedServerNames : public ConfigException
	{
		ConfigExceptionExpectedServerNames() : ConfigException("Config exception: Expected server_names"){};
	};
	struct ConfigExceptionExpectedLocations : public ConfigException
	{
		ConfigExceptionExpectedLocations() : ConfigException("Config exception: Expected locations"){};
	};
	struct ConfigExceptionExpectedErrorPages : public ConfigException
	{
		ConfigExceptionExpectedErrorPages() : ConfigException("Config exception: Expected error_pages"){};
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

	struct ConfigExceptionNeedEitherRootOrRedirect : public ConfigException
	{
		ConfigExceptionNeedEitherRootOrRedirect() : ConfigException("Config exception: Need either root or redirect"){};
	};
	struct ConfigExceptionCantHaveOtherOtherPropertiesWithRedirect : public ConfigException
	{
		ConfigExceptionCantHaveOtherOtherPropertiesWithRedirect() : ConfigException("Config exception: Can't have other properties with redirect"){};
	};
	struct ConfigExceptionCantHaveBothIndexAndAutoindex : public ConfigException
	{
		ConfigExceptionCantHaveBothIndexAndAutoindex() : ConfigException("Config exception: Can't have both index and autoindex"){};
	};

	struct ConfigExceptionLocationNeedsToStartWithSlash : public ConfigException
	{
		ConfigExceptionLocationNeedsToStartWithSlash() : ConfigException("Config exception: Location needs to start with '/'"){};
	};

	struct ConfigExceptionInvalidErrorPageCode : public ConfigException
	{
		ConfigExceptionInvalidErrorPageCode() : ConfigException("Config exception: Invalid error page code"){};
	};

	struct ConfigExceptionUnknownKey : public ConfigException
	{
		ConfigExceptionUnknownKey() : ConfigException("Config exception: Unknown key"){};
	};

	struct ConfigExceptionDuplicateLocationInServer : public ConfigException
	{
		ConfigExceptionDuplicateLocationInServer() : ConfigException("Config exception: Duplicate location in server"){};
	};
	struct ConfigExceptionConflictingServerNameOnListen : public ConfigException
	{
		ConfigExceptionConflictingServerNameOnListen() : ConfigException("Config exception: Conflicting server name on listen"){};
	};
};
