#include "Config.hpp"
#include "../Utils.hpp"
#include "JSON.hpp"
#include "Node.hpp"

Config::Config(void) : servers(), port_to_server_index() {}

void Config::init(const JSON &json)
{
	size_t server_index = 0;
	for (const auto &server_it : json.root.getObject().at("servers").getArray())
	{
		ServerDirective server_directive{};

		const std::map<std::string, Node> &server = server_it.getObject();
		for (const auto &server_property_it : server)
		{
			if (server.find("connection_queue_length") == server.end())
			{
				throw ConfigExceptionExpectedConnectionQueueLength();
			}
			if (server.find("client_max_body_size") == server.end())
			{
				throw ConfigExceptionExpectedClientMaxBodySize();
			}
			if (server.find("ports") == server.end())
			{
				throw ConfigExceptionExpectedListen();
			}

			const std::string &key = server_property_it.first;

			if (key == "connection_queue_length")
			{
				server_directive.connection_queue_length = server_property_it.second.getInteger();
				// TODO: Test what happens with listen() if we allow 0.
				// TODO: If it works, lower the limit here along with changing the exception's name
				if (server_directive.connection_queue_length < 1)
				{
					throw ConfigExceptionConnectionQueueLengthIsSmallerThanOne();
				}
			}
			else if (key == "client_max_body_size")
			{
				server_directive.client_max_body_size = server_property_it.second.getInteger();
				// TODO: Do we maybe want a higher minimum value?
				if (server_directive.client_max_body_size < 0)
				{
					throw ConfigExceptionClientMaxBodySizeIsSmallerThanZero();
				}
			}
			else if (key == "ports")
			{
				for (const auto &port_node : server_property_it.second.getArray())
				{
					const size_t port_size_t = port_node.getInteger();
					if (port_size_t > std::numeric_limits<uint16_t>::max())
					{
						throw ConfigExceptionPortIsHigherThan65535();
					}
					const uint16_t port = port_size_t;

					server_directive.ports.push_back(port);
					auto pair = port_to_server_index.emplace(
						port,
						server_index);

					// If the key was already present
					if (!pair.second)
					{
						throw ConfigExceptionDuplicatePort();
					}
				}
			}
			else if (key == "server_names")
			{
				for (const auto &server_name_node : server_property_it.second.getArray())
				{
					const std::string &server_name = server_name_node.getString();

					server_directive.server_names.push_back(server_name);
				}
			}
			// else if (key == "http_redirection") // TODO: Willen we dit er weer in zetten?
			// {
			// 	server_directive.http_redirection = server_property_it.second.getString();
			// }
			else if (key == "locations")
			{
				for (const auto &location_node : server_property_it.second.getObject())
				{
					const auto &location_object = location_node.second.getObject();

					if (location_object.find("autoindex") != location_object.end() && location_object.find("index") != location_object.end())
					{
						throw ConfigExceptionBothAutoindexAndIndex();
					}

					LocationDirective location_directive{};

					location_directive.uri = location_node.first;

					for (const auto &location_property_it : location_object)
					{
						const std::string &property_key = location_property_it.first;

						const Node &property_value = location_property_it.second;
						if (property_key == "get_allowed")
						{
							location_directive.get_allowed = property_value.getBoolean();
						}
						else if (property_key == "post_allowed")
						{
							location_directive.post_allowed = property_value.getBoolean();
						}
						else if (property_key == "delete_allowed")
						{
							location_directive.delete_allowed = property_value.getBoolean();
						}
						else if (property_key == "autoindex")
						{
							location_directive.autoindex = property_value.getBoolean();
						}
						else if (property_key == "index")
						{
							location_directive.index = property_value.getString();
						}
						else if (property_key == "root")
						{
							location_directive.root = property_value.getString();
						}
					}
					server_directive.locations.push_back(location_directive);
				}
			}
			else if (key == "error_pages")
			{
				for (const auto &error_page_node : server_property_it.second.getObject())
				{
					int error_code;
					if (!Utils::parseNumber(error_page_node.first, error_code, std::numeric_limits<int>::max()))
					{
						throw ConfigExceptionInvalidErrorPageCode();
					}

					const std::string &page_path = error_page_node.second.getString();

					server_directive.error_pages.emplace(static_cast<Status::Status>(error_code), page_path);
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
}
