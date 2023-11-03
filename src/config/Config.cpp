#include "Config.hpp"
#include "../Utils.hpp"
#include "JSON.hpp"
#include "Node.hpp"

Config::Config(void)
	: servers(),
	  connection_queue_length(),
	  client_max_body_size()
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

			const std::string &key = server_property_it.first;

			if (key == "listen")
			{
				for (const auto &listen_node : server_property_it.second.getArray())
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

					std::string port_str = listen.substr(colon_index + 1);

					uint16_t port;
					if (!Utils::parseNumber(port_str, port, static_cast<uint16_t>(65535))) throw ConfigExceptionInvalidPortValue();

					server_directive.listen.emplace_back(ListenEntry{address, port});
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

					bool contains_autoindex = location_object.contains("autoindex");
					bool contains_index = location_object.contains("index");
					bool contains_redirect = location_object.contains("redirect");

					if (contains_autoindex + contains_index + contains_redirect > 1)
					{
						throw ConfigExceptionExclusivePropertiesPresent();
					}

					LocationDirective location_directive{};

					location_directive.uri = location_node.first;

					for (const auto &location_property_it : location_object)
					{
						const std::string &location_property_key = location_property_it.first;
						const Node &location_property_value = location_property_it.second;

						if (location_property_key == "cgi_settings")
						{
							CGISettingsDirective &cgi_settings_directive = location_directive.cgi_settings;

							location_directive.is_cgi_directory = true;

							const auto &cgi_settings_object = location_property_value.getObject();

							if (!cgi_settings_object.contains("cgi_execve_path"))
							{
								throw ConfigExceptionMissingCGISettingsProperty();
							}

							for (const auto &cgi_setting : cgi_settings_object)
							{
								const std::string &settings_property_key = cgi_setting.first;
								const std::string &settings_property_value = cgi_setting.second.getString();

								if (settings_property_key == "cgi_execve_path")
								{
									cgi_settings_directive.cgi_execve_path = settings_property_value;

									if (!cgi_settings_object.contains("cgi_execve_argv0"))
									{
										cgi_settings_directive.cgi_execve_argv0 = settings_property_value;
									}
								}
								else if (settings_property_key == "cgi_execve_argv0")
								{
									cgi_settings_directive.cgi_execve_argv0 = settings_property_value;
								}
								else
								{
									throw ConfigExceptionUnknownKey();
								}
							}
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
