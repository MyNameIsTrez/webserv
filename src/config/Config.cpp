#include "Config.hpp"

#include "../Logger.hpp"
#include "../Utils.hpp"

#include "JSON.hpp"
#include "Node.hpp"

#include <set>

Config::Config(void)
	: connection_queue_length(),
	  client_max_body_size(),
	  servers(),
	  bind_info_to_server_indices()
{
}

void Config::init(const JSON &json)
{
	const auto &root_object = json.root.getObject();

	_parseConnectionQueueLength(root_object);

	_parseClientMaxBodySize(root_object);

	size_t server_index = 0;
	for (const auto &server_it : root_object.at("servers").getArray())
	{
		ServerDirective server_directive{};

		const std::unordered_map<std::string, Node> &server = server_it.getObject();
		for (const auto &server_property_it : server)
		{
			if (!server.contains("listen"))
			{
				throw ConfigExceptionExpectedListen();
			}
			if (!server.contains("server_names"))
			{
				throw ConfigExceptionExpectedServerNames();
			}
			if (!server.contains("locations"))
			{
				throw ConfigExceptionExpectedLocations();
			}
			if (!server.contains("error_pages"))
			{
				throw ConfigExceptionExpectedErrorPages();
			}

			const std::string &key = server_property_it.first;
			const Node &value = server_property_it.second;

			if (key == "listen")
			{
				for (const auto &listen_node : value.getArray())
				{
					server_directive.listen.push_back(_parseListen(listen_node));
				}
			}
			else if (key == "server_names")
			{
				for (const auto &server_name_node : value.getArray())
				{
					server_directive.server_names.push_back(server_name_node.getString());
				}
			}
			else if (key == "locations")
			{
				for (const auto &location_node : value.getObject())
				{
					server_directive.locations.push_back(_parseLocation(location_node));
				}
			}
			else if (key == "error_pages")
			{
				for (const auto &error_page_node : value.getObject())
				{
					int error_code;
					if (!Utils::parseNumber(error_page_node.first, error_code, std::numeric_limits<int>::max()))
					{
						throw ConfigExceptionInvalidErrorPageCode();
					}

					const std::string &page_path = error_page_node.second.getString();

					server_directive.error_pages.emplace(Status::Status(error_code), page_path);
				}
			}
			else
			{
				throw ConfigExceptionUnknownKey();
			}
		}

		servers.push_back(server_directive);
		server_index++;
	}

	_fillBindInfoToServerIndices();
}

void Config::_parseConnectionQueueLength(const std::unordered_map<std::string, Node> &root_object)
{
	if (!root_object.contains("connection_queue_length"))
	{
		throw ConfigExceptionExpectedConnectionQueueLength();
	}

	size_t connection_queue_length_size_t = root_object.at("connection_queue_length").getInteger();
	if (connection_queue_length_size_t > std::numeric_limits<int>::max())
	{
		throw ConfigExceptionConnectionQueueLengthIsTooHigh();
	}
	connection_queue_length = connection_queue_length_size_t;

	if (connection_queue_length < 1)
	{
		throw ConfigExceptionConnectionQueueLengthIsSmallerThanOne();
	}
}

void Config::_parseClientMaxBodySize(const std::unordered_map<std::string, Node> &root_object)
{
	if (!root_object.contains("client_max_body_size"))
	{
		throw ConfigExceptionExpectedClientMaxBodySize();
	}

	client_max_body_size = root_object.at("client_max_body_size").getInteger();

	if (client_max_body_size < 0)
	{
		throw ConfigExceptionClientMaxBodySizeIsSmallerThanZero();
	}
}

Config::ListenEntry Config::_parseListen(const Node &listen_node)
{
	const std::string &listen = listen_node.getString();

	size_t colon_index = listen.find(":");
	if (colon_index == listen.npos)
	{
		throw ConfigExceptionListenColonIsRequired();
	}

	if (colon_index == 0) throw ConfigExceptionMissingAddressBeforePortColon();

	std::string address = listen.substr(0, colon_index);

	if (colon_index == listen.size() - 1) throw ConfigExceptionNoPortAfterColon();

	std::string port = listen.substr(colon_index + 1);

	return {address, port};
}

Config::LocationDirective Config::_parseLocation(const std::pair<std::string, Node> &location_node)
{
	const auto &location_object = location_node.second.getObject();

	if (!location_object.contains("root") && !location_object.contains("redirect"))
	{
		throw ConfigExceptionNeedEitherRootOrRedirect();
	}
	if (location_object.contains("redirect") && location_object.size() > 1)
	{
		throw ConfigExceptionCantHaveOtherOtherPropertiesWithRedirect();
	}
	if (location_object.contains("index") && location_object.contains("autoindex"))
	{
		throw ConfigExceptionCantHaveBothIndexAndAutoindex();
	}

	LocationDirective location_directive{};

	location_directive.uri = location_node.first;

	if (location_directive.uri.front() != '/')
	{
		throw ConfigExceptionLocationNeedsToStartWithSlash();
	}

	for (const auto &location_property_it : location_object)
	{
		const std::string &location_property_key = location_property_it.first;
		const Node &location_property_value = location_property_it.second;

		if (location_property_key == "cgi_execve_path")
		{
			location_directive.cgi_execve_path = location_property_value.getString();
		}
		else if (location_property_key == "get_allowed")
		{
			location_directive.get_allowed = location_property_value.getBoolean();
		}
		else if (location_property_key == "post_allowed")
		{
			location_directive.post_allowed = location_property_value.getBoolean();
		}
		else if (location_property_key == "delete_allowed")
		{
			location_directive.delete_allowed = location_property_value.getBoolean();
		}
		else if (location_property_key == "autoindex")
		{
			location_directive.autoindex = location_property_value.getBoolean();
		}
		else if (location_property_key == "index")
		{
			location_directive.index = location_property_value.getString();
		}
		else if (location_property_key == "redirect")
		{
			location_directive.redirect = location_property_value.getString();
		}
		else if (location_property_key == "root")
		{
			location_directive.root = location_property_value.getString();
		}
		else
		{
			throw ConfigExceptionUnknownKey();
		}
	}

	return location_directive;
}

void Config::_fillBindInfoToServerIndices()
{
	// Used to throw if we see the same server name twice
	std::map<BindInfo, std::set<std::string>> names_of_bind_info;

	size_t server_index = 0;
	for (const auto &server : servers)
	{
		std::set<BindInfo> bind_infos_in_server;

		for (const auto &listen_entry : server.listen)
		{
			Logger::info(std::string("Listen entry: address ") + listen_entry.address + " with port " + listen_entry.port);

			addrinfo hint{};
			hint.ai_family = AF_INET;
			hint.ai_socktype = SOCK_STREAM;
			protoent *proto = getprotobyname("tcp");
			if (proto == NULL) throw Utils::SystemException("getprotobyname");
			hint.ai_protocol = proto->p_proto;

			addrinfo *result;
			int status = getaddrinfo(listen_entry.address.c_str(), listen_entry.port.c_str(), &hint, &result);
			if (status != 0) throw Utils::SystemException("getaddrinfo", gai_strerror(status));
			if (result == NULL) throw Utils::SystemException("getaddrinfo");

			// TODO: Is it possible for there not to be a single result while status == 0?
			BindInfo bind_info;
			sockaddr_in *ai_addr = reinterpret_cast<sockaddr_in *>(result->ai_addr);
			bind_info.s_addr = ai_addr->sin_addr.s_addr;
			bind_info.sin_port = ai_addr->sin_port;

			freeaddrinfo(result);

			if (bind_infos_in_server.contains(bind_info)) throw ConfigExceptionDuplicateLocationInServer();
			bind_infos_in_server.insert(bind_info);

			// Deliberately creates the vector if it is missing
			bind_info_to_server_indices[bind_info].push_back(server_index);

			for (const std::string &server_name : server.server_names)
			{
				// Deliberately creates the vector if it is missing
				if (names_of_bind_info[bind_info].contains(server_name)) throw ConfigExceptionConflictingServerNameOnListen();

				names_of_bind_info.at(bind_info).insert(server_name);
			}
		}

		server_index++;
	}
}
