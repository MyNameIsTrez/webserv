#include "Config.hpp"
#include "Token.hpp"

Config::Config(void)
{
}

Config::~Config(void)
{
}

void Config::save_type(std::string type, std::string value)
{
	if (type == "max_connections")
		return save_max_connections(value);
	if (type == "default_file")
		return save_default_file(value);
	throw InvalidLineException();
}

unsigned long check_digit(std::string input)
{
	unsigned long result = 0;
	for (size_t i = 0; i < input.size(); i++)
	{
		if (input[i] < '0' || input[i] > '9')
			throw InvalidLineException();

		result *= 10;
		result += (unsigned long)(input[i] - '0');
	}
	return (result);
}
void Config::save_max_connections(std::string value)
{

	_max_connections = check_digit(value);
	// _max_connections = std::stoul(line.substr(line.find('=') + 1));
}

void Config::save_default_file(std::string value)
{
	_default_file = value;
}

void Config::init(std::istream &config)
{
	std::string line;
	while (getline(config, line))
	{
		// TODO: Remove
		// if (line.find("a") != line.npos)
		// 	abort();
		if (line.find_first_not_of('\t') == line.npos)
			continue;
		if (line.find("server {") != line.npos)
		{
			new_server(line, config);
			// std::cout << "			END OF SERVER" << std::endl;
		}
		else
		{ // TODO: hier ook tokenization gebruiken
			std::string type;
			std::string value;
			size_t ti = 0; // token_index
			std::vector<Token> t_line = tokenize_line(line);
			if (t_line.at(ti).type == WHITESPACE)
				ti++;
			if (ti < t_line.size() && t_line.at(ti).type == WORD)
			{
				if (t_line.at(ti).str.c_str()[0] == '#')
				{
					std::cout << line << " THIS IS A COMMENT" << std::endl;
					continue;
				}
				else
					type = t_line.at(ti).str;
				ti++;
			}
			else
				throw InvalidLineException();
			if (ti < t_line.size() && t_line.at(ti).type == WHITESPACE)
				ti++;
			if (ti < t_line.size() && t_line.at(ti).type == EQUALS)
				ti++;
			else
				throw InvalidLineException();
			if (ti < t_line.size() && t_line.at(ti).type == WHITESPACE)
				ti++;
			if (ti < t_line.size() && t_line.at(ti).type == WORD)
				value = t_line.at(ti).str;
			else
				throw InvalidLineException();

			// if (line.find('=') == line.npos)
			// {
			// 	std::cout << "<" << line << ">" << std::endl;
			// 	throw InvalidLineException();
			// }
			// line.erase(remove_if(line.begin(), line.end(), isspace), line.end());
			// std::string type = line.substr(0, line.find('='));
			// std::cout << type << std::endl;
			// if (type == "\n")
			// {
			// 	throw EmptyTypeException();
			// }
			save_type(type, value);
		}
	}
}

void Config::save_error_pages(std::string line, ServerData *new_server) // TODO: hier ook tokenization en check_digit gebruiktn
{
	size_t i = line.find("error_page");
	if (i != line.npos)
	{
		while (line[i] != '\0')
		{
			if (line[i] == '=')
				break;
			int page = 0;
			if (isdigit(line.c_str()[i]) != 0) // checken of er uberhaupt error codes in de config staan en of er een pad beschreven is. Zo niet, exception throwen
			{
				while (isdigit(line.c_str()[i]) != 0)
				{
					page *= 10;
					page += line.c_str()[i] - '0';
					i++;
				}
				new_server->error_pages.emplace(static_cast<Status::Status>(page), line.substr(line.find('=') + 2));
			}
			i++;
		}
	}
}

void print_vector(std::string prefix, std::vector<std::string> nums)
{
	std::cout << prefix;
	for (size_t i = 0; i < nums.size(); i++)
	{
		std::cout << " " << nums.at(i);
	}
	std::cout << std::endl;
}

PageData Config::save_page(std::string line, std::istream &config)
{
	// std::cout << "LOCATION BEGINNING" << std::endl;
	size_t page_start = line.find('/');
	if (page_start == line.npos)
	{
		throw InvalidLineException();
	}
	size_t page_end = line.find('{');
	if (page_end == line.npos)
	{
		throw InvalidLineException();
	}
	PageData new_page;
	new_page.page_path = line.substr(page_start, page_end - page_start);
	while (getline(config, line))
	{
		if (line.find('}') != line.npos)
			break;
		if (line[0] != '\0')
		{
			if (line.find('=') == line.npos)
				throw InvalidLineException();
			std::string value = line.substr(line.find('=') + 1);
			line.erase(remove_if(line.begin(), line.end(), isspace), line.end());
			std::string type = line.substr(0, line.find('='));
			std::cout << type << " = " << value << std::endl;
			if (type == "allow_methods")
			{
				std::stringstream ss(value);
				std::istream_iterator<std::string> begin(ss);
				std::istream_iterator<std::string> end;
				std::vector<std::string> vstrings(begin, end);
				new_page.allowed_methods = vstrings;
				print_vector("allow_methods =", new_page.allowed_methods);
			}
			else if (type == "autoindex")
			{
				if (value == "on")
					new_page.autoindex = true;
				else
					new_page.autoindex = false; // TODO: wordt nu ook op false gezet bij gibberish
			}
			else if (type == "index_file")
			{
				new_page.index_file = value;
			}
			else if (type == "root_path")
			{
				new_page.root = value;
			}
			else if (type == "cgi_path")
			{
				std::stringstream ss(value);
				std::istream_iterator<std::string> begin(ss);
				std::istream_iterator<std::string> end;
				std::vector<std::string> vstrings(begin, end);
				new_page.cgi_paths = vstrings;
				// print_vector("cgi_paths =", new_page.cgi_paths);
			}
			else if (type == "cgi_ext")
			{
				std::stringstream ss(value);
				std::istream_iterator<std::string> begin(ss);
				std::istream_iterator<std::string> end;
				std::vector<std::string> vstrings(begin, end);
				new_page.cgi_ext = vstrings;
				// print_vector("cgi_extensions =", new_page.cgi_ext);
			}
			else
				throw InvalidLineException();
		}
	}
	// std::cout << "LOCATION: " << new_page.page_path << std::endl;
	return new_page;
}

// "new_server" zou ik kunnen rewriten zodat het meteen in de "serverdata" gezet wordt
// dan hoeven functies zoals save_page en save_error_pages geen "new_server" mee te krijgen
void Config::new_server(std::string line, std::istream &config)
{
	// std::cout << "			NEW_SERVER" << std::endl;
	ServerData new_server;

	int unclosed = 0;
	std::cout << line << std::endl;
	while (getline(config, line))
	{
		if (line[0] != '\0')
		{
			std::cout << line << std::endl;
			if (line.find('}') != line.npos)
			{
				if (unclosed == 0)
					break;
				else
					unclosed--;
			}
			else if (line.find('{') != line.npos)
				unclosed++;
			if (line.find("location") != line.npos)
			{
				new_server.page_data.push_back(save_page(line, config));
				unclosed--;
			}
			else if (line.find('=') != line.npos)
			{
				if (line.find("error_page") != line.npos) // is nog niet beschermd
					save_error_pages(line, &new_server);
				else
				{
					std::string type;
					std::string value;
					size_t ti = 0; // token_index
					std::vector<Token> t_line = tokenize_line(line);
					if (t_line.at(ti).type == WHITESPACE)
						ti++;
					if (ti < t_line.size() && t_line.at(ti).type == WORD)
					{
						if (t_line.at(ti).str.c_str()[0] == '#')
						{
							std::cout << line << " THIS IS A COMMENT" << std::endl;
							continue;
						}
						else
							type = t_line.at(ti).str;
						ti++;
					}
					else
						throw InvalidLineException();
					if (ti < t_line.size() && t_line.at(ti).type == WHITESPACE)
						ti++;
					if (ti < t_line.size() && t_line.at(ti).type == EQUALS)
						ti++;
					else
						throw InvalidLineException();
					if (ti < t_line.size() && t_line.at(ti).type == WHITESPACE)
						ti++;
					if (ti < t_line.size() && t_line.at(ti).type == WORD)
						value = t_line.at(ti).str;
					else
						throw InvalidLineException();

					// line.erase(remove_if(line.begin(), line.end(), isspace), line.end());
					// std::string type = line.substr(0, line.find('='));
					// std::cout << type << std::endl;
					// std::string value = line.substr(line.find('=') + 1);
					// std::cout << "value of " << type << ": " << value << std::endl;
					if (value == "")
					{
						throw EmptyTypeException();
					}
					if (type == "server_name")
						new_server.server_name = value;
					else if (type == "listen")
						new_server.ports.push_back((int)check_digit(value));
					else if (type == "root_path")
						new_server.root_path = value;
					else if (type == "index_file")
						new_server.index_file = value;
					else if (type == "client_max_body_size")
						new_server.client_max_body_size = check_digit(value);
					else if (type == "http_redirection")
						new_server.http_redirection = value;
					else
					{
						std::cout << "Error on line:" << line << std::endl;
						throw InvalidLineException();
						exit(EXIT_FAILURE);
					}
				}
			}
		}
	}
	serverdata.push_back(new_server);
	return;
}

void Config::print_config_info(void)
{
	std::cout << std::endl;
	std::cout << std::endl;
	std::cout << "max_connections: " << _max_connections << std::endl;
	std::cout << "default_file: " << _default_file << std::endl;

	std::cout << std::endl;
	for (size_t i = 0; i < serverdata.size(); i++)
	{
		std::cout << "server_name: " << serverdata.at(i).server_name << std::endl;
		std::cout << "server ports: ";
		for (size_t j = 0; j < serverdata.at(i).ports.size(); j++)
		{
			std::cout << serverdata.at(i).ports.at(j) << ", ";
		}
		std::cout << std::endl;
		std::cout << "root_path: " << serverdata.at(i).root_path << std::endl;
		std::cout << "index_file: " << serverdata.at(i).index_file << std::endl;
		std::cout << "client_max_body_size: " << serverdata.at(i).client_max_body_size << std::endl;
		std::cout << "http_redirection: " << serverdata.at(i).http_redirection << std::endl;
		for (std::map<Status::Status, std::string>::iterator it = serverdata.at(i).error_pages.begin(); it != serverdata.at(i).error_pages.end(); ++it)
		{
			std::cout << "error page: " << it->first << ": " << it->second << std::endl;
		}
		for (size_t h = 0; h < serverdata.at(i).page_data.size(); h++)
		{
			std::cout << "page_path: " << serverdata.at(i).page_data.at(h).page_path << std::endl;
			std::cout << "allowed_methods: ";
			for (size_t k = 0; k < serverdata.at(i).page_data.at(h).allowed_methods.size(); k++)
			{
				std::cout << serverdata.at(i).page_data.at(h).allowed_methods.at(k) << ", ";
			}
			std::cout << std::endl;
			std::cout << "autoindex: " << serverdata.at(i).page_data.at(h).autoindex << std::endl;
			std::cout << "index_file: " << serverdata.at(i).page_data.at(h).index_file << std::endl;
			std::cout << "root: " << serverdata.at(i).page_data.at(h).root << std::endl;
			std::cout << "cgi_paths: ";
			for (size_t l = 0; l < serverdata.at(i).page_data.at(h).cgi_paths.size(); l++)
			{
				std::cout << serverdata.at(i).page_data.at(h).cgi_paths.at(l) << ", ";
			}
			std::cout << std::endl;
			std::cout << "cgi_extensions: ";
			for (size_t m = 0; m < serverdata.at(i).page_data.at(h).cgi_ext.size(); m++)
			{
				std::cout << serverdata.at(i).page_data.at(h).cgi_ext.at(m) << ", ";
			}
			std::cout << std::endl;
			std::cout << std::endl;
		}
	}
}
