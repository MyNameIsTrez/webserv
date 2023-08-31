#include "ClientData.hpp"
#include <unistd.h>
#include <vector>
#include <assert.h> // TODO: DELETE WHEN FINISHING PROJECT

#define MAX_RECEIVED_LEN 5

/*	Orthodox Canonical Form */

ClientData::ClientData(void)
	: read_state(HEADER),
	  respond_index(0),
	  _content_length(0),
	  _fd(0)
{
}

ClientData::ClientData(ClientData const &src)
	: read_state(src.read_state),
	  request_method(src.request_method),
	  path(src.path),
	  body(src.body),
	  respond_index(0),
	  _content_length(0),
	  _fd(src._fd)
{
}

ClientData::~ClientData(void)
{
}

ClientData &ClientData::operator=(ClientData const &src)
{
	if (this == &src)
		return (*this);
	this->read_state = src.read_state;
	this->request_method = src.request_method;
	this->path = src.path;
	this->body = src.body;
	this->respond_index = src.respond_index;
	this->_header = src._header;
	this->_content_length = src._content_length;
	this->_fd = src._fd;
	return (*this);
}

/*	Other constructors */

ClientData::ClientData(int client_fd) : read_state(HEADER), respond_index(0), _content_length(0), _fd(client_fd)
{
}

/*	Member functions */

bool ClientData::parseStartLine(std::string line)
{
	// Find and set the request method
	size_t request_method_end_pos = line.find(" ");
	if (request_method_end_pos == std::string::npos)
		return (false);
	this->request_method = line.substr(0, request_method_end_pos);
	request_method_end_pos++;

	// Find and set the path
	size_t path_end_pos = line.find_last_of(" ");
	if (path_end_pos == std::string::npos || path_end_pos == request_method_end_pos - 1)
		return (false);
	this->path = line.substr(request_method_end_pos, path_end_pos - request_method_end_pos);
	path_end_pos++;

	// Set and validate the protocol
	// // Validate "HTTP/"
	this->protocol = line.substr(path_end_pos);
	if (this->protocol.find("HTTP/", 0, 5) == std::string::npos)
		return (false);

	// // Validate major version
	size_t i;
	for (i = 5; i < this->protocol.size(); i++)
	{
		if (this->protocol[i] < '0' || this->protocol[i] > '9')
			break;
	}

	// // Validate version seperator
	if (i == 5 || i == this->protocol.size() || this->protocol[i] != '.')
		return (false);

	// // Validate minor version
	i++;
	size_t j = i;
	for (; i < this->protocol.size(); i++)
	{
		if (this->protocol[i] < '0' || this->protocol[i] > '9')
			break;
	}
	if (j == i || i != this->protocol.size())
		return (false);
	return (true);
}

// // TODO: Put in debug.cpp or smth
// #include <sstream>
// #include <iostream>
// #include <iomanip>
// std::string ToHex(const std::string &s, bool upper_case = true)
// {
// 	std::stringstream ret;

// 	for (std::string::size_type i = 0; i < s.length(); ++i)
// 		ret << std::hex << std::setfill('0') << std::setw(2) << (upper_case ? std::uppercase : std::nouppercase) << (int)s[i];

// 	return ret.str();
// }

bool ClientData::parseHeaders(void)
{
	std::vector<std::string> split;
	size_t pos;
	size_t start = 0;

	// Delete the trailing "\r\n"
	this->_header.erase(this->_header.size() - 2);

	// std::cerr << "\"" << ToHex(this->_header) << "\"" << std::endl;

	while ((pos = this->_header.find("\r\n", start)) != std::string::npos)
	{
		split.push_back(this->_header.substr(start, pos - start));
		start = pos + 2;
	}

	if (!this->parseStartLine(split[0]))
		return (false);

	// TODO: Look what to do with HTTP_ thingy (See page 11 & 19 in cgi rfc)
	// Loop for every header
	for (size_t i = 1; i < split.size(); i++)
	{
		std::string line = split[i];

		// Assign key to everything before the ':' seperator
		size_t pos = line.find(":");
		if (pos == std::string::npos)
			return (false);
		std::string key = line.substr(0, pos);

		// Check if key is a duplicate
		if (this->header_map.find(key) != this->header_map.end())
			return (false);

		// Capitalizing letters and replace '-' with '_'
		for (size_t i = 0; i < key.size(); i++)
		{
			key[i] = toupper(key[i]);
			if (key[i] == '-')
				key[i] = '_';
		}

		// Skip all whitespaces (' ' and '\t') before value
		pos++;
		while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
			pos++;

		// Assign value to everything after the whitespaces
		std::string value = line.substr(pos);

		// Delete all tailing whitespaces from value
		for (size_t i = 0; i < value.size(); i++)
		{
			if (value[i] == ' ' || value[i] == '\t')
			{
				value.erase(i);
				break;
			}
		}

		// Add key and value to the map
		this->header_map.insert(std::pair<std::string, std::string>(key, value));
	}
	return (true);
}

bool ClientData::readSocket(void)
{
	char received[MAX_RECEIVED_LEN];
	bzero(received, MAX_RECEIVED_LEN);

	ssize_t bytes_read = read(this->_fd, received, MAX_RECEIVED_LEN);
	if (bytes_read == -1)
	{
		perror("read");
		exit(EXIT_FAILURE);
	}
	// TODO: Handle this (probably also something with setting state to DONE)
	// if (bytes_read == 0)
	// {
	// }

	if (this->read_state == HEADER)
	{
		// "\r\n\r" + "\n"
		if (this->_header.size() >= 3 && this->_header[this->_header.size() - 3] == '\r' && this->_header[this->_header.size() - 2] == '\n' && this->_header[this->_header.size() - 1] == '\r' && received[0] == '\n')
		{
			this->_header.append(received, 0, 1);
			this->body.append(received, 1, bytes_read - 1);
			this->read_state = BODY;
			if (!parseHeaders())
				return (false);
		}
		// "\r\n" + "\r\n"
		else if (this->_header.size() >= 2 && bytes_read >= 2 && this->_header[this->_header.size() - 2] == '\r' && this->_header[this->_header.size() - 1] == '\n' && received[0] == '\r' && received[1] == '\n')
		{
			this->_header.append(received, 0, 2);
			this->body.append(received, 2, bytes_read - 2);
			this->read_state = BODY;
			if (!parseHeaders())
				return (false);
		}
		// "\r" + "\n\r\n"
		else if (this->_header.size() >= 1 && bytes_read >= 3 && this->_header[this->_header.size() - 1] == '\r' && received[0] == '\n' && received[1] == '\r' && received[2] == '\n')
		{
			this->_header.append(received, 0, 3);
			this->body.append(received, 3, bytes_read - 3);
			this->read_state = BODY;
			if (!parseHeaders())
				return (false);
		}
		else
		{
			char const rnrn[] = "\r\n\r\n";
			char *ptr = std::search(received, received + bytes_read, rnrn, rnrn + sizeof(rnrn) - 1);

			// If no \r\n\r\n found
			if (ptr == received + bytes_read)
			{
				this->_header += std::string(received, bytes_read);
			}
			// If \r\n\r\n found
			else
			{
				this->_header.append(received, ptr + 4);					   // Include "\r\n\r\n"
				this->body.append(ptr + 4, bytes_read - (ptr + 4 - received)); // Skip "\r\n\r\n"
				this->read_state = BODY;
				if (!parseHeaders())
					return (false);
			}
		}
	}
	else if (this->read_state == BODY)
	{
		body += std::string(received, bytes_read);
	}
	else
	{
		// Should be unreachable
		assert(false); // TODO: REMOVE BEFORE HANDING IN
	}
	return (true);
}
