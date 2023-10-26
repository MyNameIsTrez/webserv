#include "Config.hpp"

Config::Config(const JSON &json)
{
	for (const auto &server : json.root.getObject().at("servers").getArray())
	{
		// TODO: hier binnen nog connection_queue_length en default file in handlen
		ServerDirective server_directive;

		for (const auto &server_property_it : server.getObject())
		{
			const std::string &key = server_property_it.first;

			if (key == "listen")
			{
				const Node &ports_node = server_property_it.second;

				for (const auto &port_node : ports_node.getArray())
				{
					const uint16_t &port = port_node.getInteger();

					server_directive.ports.push_back(port);
				}
			}
			else if (key == "server_names")
			{
				const Node &server_names_node = server_property_it.second;

				for (const auto &server_name_node : server_names_node.getArray())
				{
					const std::string &server_name = server_name_node.getString();

					server_directive.server_names.push_back(server_name);
				}
			}
			else if (key == "client_max_body_size")
			{
				server_directive.client_max_body_size = server_property_it.second.getInteger();
			}
			else if (key == "http_redirection")
			{
				server_directive.http_redirection = server_property_it.second.getString();
			}
			else if (key == "locations")
			{
				const Node &locations_node = server_property_it.second;

				for (const auto &location_node : locations_node.getObject())
				{
					LocationDirective location_directive;

					location_directive.uri = location_node.first;

					const Node &locations = location_node.second;
					for (const auto &location : locations.getObject())
					{
						const Node &location_datas = location.second;

						std::string location_key = location.first;
						for (const auto &location_data : location_datas.getObject())
						{
							if (location_key == "get_allowed")
							{
								// location_data
							}
							else if (location_key == "post_allowed")
							{
							}
							else if (location_key == "delete_allowed")
							{
							}
							else if (location_key == "autoindex")
							{
							}
							else if (location_key == "index")
							{
							}
							else if (location_key == "root")
							{
							}
						}
					}
					server_directive.locations.push_back(location_directive);
				}
			}
			else
			{
				throw ConfigExceptionUnknownKey();
			}
		}

		servers.push_back(server_directive);
	}
}
