#include "Client.hpp"

#include <algorithm>
#include <assert.h> // TODO: DELETE WHEN FINISHING PROJECT
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <poll.h>
#include <unistd.h>
#include <vector>

#define MAX_RECEIVED_LEN 50

/*	Orthodox Canonical Form */

Client::Client(void)
	: client_read_state(ClientReadState::HEADER),
	  cgi_write_state(CGIWriteState::NOT_WRITING),
	  cgi_read_state(CGIReadState::NOT_READING),
	  client_write_state(ClientWriteState::NOT_WRITING),
	  request_method(),
	  path(),
	  protocol(),
	  header_map(),
	  body(),
	  body_index(0),
	  response(),
	  response_index(0),
	  client_fd(0),
	  server_to_cgi_fd(-1),
	  cgi_to_server_fd(-1),
	  cgi_reaped(false),
	  cgi_to_server_hangup(false),
	  server_to_cgi_hangup(false),
	  _header(),
	  _content_length(0)
{
}

Client::Client(Client const &src)
	: client_read_state(src.client_read_state),
	  cgi_write_state(src.cgi_write_state),
	  cgi_read_state(src.cgi_read_state),
	  client_write_state(src.client_write_state),
	  request_method(src.request_method),
	  path(src.path),
	  protocol(src.protocol),
	  header_map(src.header_map), // TODO: Check whether this correctly copies
	  body(src.body),
	  body_index(src.body_index),
	  response(src.body),
	  response_index(0), // TODO: Why does this differ from what is done in the copy assignment overload?
	  client_fd(src.client_fd),
	  server_to_cgi_fd(src.server_to_cgi_fd),
	  cgi_to_server_fd(src.cgi_to_server_fd),
	  cgi_reaped(src.cgi_reaped),
	  cgi_to_server_hangup(src.cgi_to_server_hangup),
	  server_to_cgi_hangup(src.server_to_cgi_hangup),
	  _header(src._header),
	  _content_length(0) // TODO: Why does this differ from what is done in the copy assignment overload?
{
}

Client::~Client(void)
{
}

Client &Client::operator=(Client const &src)
{
	if (this == &src)
		return *this;
	this->client_read_state = src.client_read_state;
	this->cgi_write_state = src.cgi_write_state;
	this->cgi_read_state = src.cgi_read_state;
	this->client_write_state = src.client_write_state;
	this->request_method = src.request_method;
	this->path = src.path;
	this->protocol = src.protocol;
	this->header_map = src.header_map; // TODO: Check whether this correctly copies
	this->body = src.body;
	this->body_index = src.body_index;
	this->response = src.response;
	this->response_index = src.response_index; // TODO: Why does this differ from what is done in the copy constructor?
	this->client_fd = src.client_fd;
	this->server_to_cgi_fd = src.server_to_cgi_fd;
	this->cgi_to_server_fd = src.cgi_to_server_fd;
	this->cgi_reaped = src.cgi_reaped,
	this->cgi_to_server_hangup = src.cgi_to_server_hangup,
	this->server_to_cgi_hangup = src.server_to_cgi_hangup,
	this->_header = src._header;
	this->_content_length = src._content_length; // TODO: Why does this differ from what is done in the copy constructor?
	return *this;
}

/*	Other constructors */

Client::Client(int client_fd)
	: client_read_state(ClientReadState::HEADER),
	  cgi_write_state(CGIWriteState::NOT_WRITING),
	  cgi_read_state(CGIReadState::NOT_READING),
	  client_write_state(ClientWriteState::NOT_WRITING),
	  request_method(),
	  path(),
	  protocol(),
	  header_map(),
	  body(),
	  body_index(0),
	  response(),
	  response_index(0),
	  client_fd(client_fd),
	  server_to_cgi_fd(-1),
	  cgi_to_server_fd(-1),
	  cgi_reaped(false),
	  cgi_to_server_hangup(false),
	  server_to_cgi_hangup(false),
	  _header(),
	  _content_length(0)
{
}

/*	Public member functions */

bool Client::readFd(std::vector<pollfd> &pfds, const std::unordered_map<int, size_t> &fd_to_pfds_index, FdType::FdType fd_type)
{
	// TODO: Remove this before the evaluation
	assert(this->cgi_read_state != CGIReadState::DONE);
	assert(this->client_write_state != ClientWriteState::DONE);

	char received[MAX_RECEIVED_LEN] = {};

	int fdx = _getFdFromFdType(fd_type);

	std::cerr << "    About to call read(" << fdx << ", received, " << MAX_RECEIVED_LEN << ") on fd_type " << fd_type << std::endl;

	// TODO: Never read past the content_length of the BODY
	ssize_t bytes_read = read(fdx, received, MAX_RECEIVED_LEN);
	if (bytes_read == -1)
	{
		perror("read");
		exit(EXIT_FAILURE);
	}
	if (bytes_read == 0)
	{
		std::cerr << "    Read 0 bytes" << std::endl;

		// TODO: Check that we reached content_length

		if (fd_type == FdType::CLIENT)
		{
			this->client_read_state = ClientReadState::DONE;

			size_t client_pfds_index = fd_to_pfds_index.at(this->client_fd);
			std::cerr << "    Disabling client POLLIN" << std::endl;
			pfds[client_pfds_index].events &= ~POLLIN;
		}
		else if (fd_type == FdType::CGI_TO_SERVER)
		{
			this->cgi_read_state = CGIReadState::DONE;

			size_t cgi_to_server_pfds_index = fd_to_pfds_index.at(this->cgi_to_server_fd);
			std::cerr << "    Disabling cgi_to_server POLLIN" << std::endl;
			pfds[cgi_to_server_pfds_index].events &= ~POLLIN;
		}

		return true;
	}

	std::cerr << "    Read " << bytes_read << " bytes:\n----------\n" << std::string(received, bytes_read) << "\n----------" << std::endl;

	if (fd_type == FdType::CLIENT)
	{
		if (this->client_read_state == ClientReadState::HEADER)
		{
			char *received_end = received + bytes_read;

			// "\r\n\r" + "\n"
			if (this->_header.size() >= 3 && this->_header[this->_header.size() - 3] == '\r' && this->_header[this->_header.size() - 2] == '\n' && this->_header[this->_header.size() - 1] == '\r' && received[0] == '\n')
			{
				this->_header.append(received, received + 1);
				this->body.append(received + 1, received_end);
				this->client_read_state = ClientReadState::BODY;
				if (!_parseHeaders())
					return false;
			}
			// "\r\n" + "\r\n"
			else if (this->_header.size() >= 2 && bytes_read >= 2 && MAX_RECEIVED_LEN >= 2 && this->_header[this->_header.size() - 2] == '\r' && this->_header[this->_header.size() - 1] == '\n' && received[0] == '\r' && received[1] == '\n')
			{
				this->_header.append(received, received + 2);
				this->body.append(received + 2, received_end);
				this->client_read_state = ClientReadState::BODY;
				if (!_parseHeaders())
					return false;
			}
			// "\r" + "\n\r\n"
			else if (this->_header.size() >= 1 && bytes_read >= 3 && MAX_RECEIVED_LEN >= 3 && this->_header[this->_header.size() - 1] == '\r' && received[0] == '\n' && received[1] == '\r' && received[2] == '\n')
			{
				this->_header.append(received, received + 3);
				this->body.append(received + 3, received_end);
				this->client_read_state = ClientReadState::BODY;
				if (!_parseHeaders())
					return false;
			}
			else
			{
				char const rnrn[] = "\r\n\r\n";
				char *ptr = std::search(received, received_end, rnrn, rnrn + sizeof(rnrn) - 1);

				// If "\r\n\r\n" isn't found in received
				if (ptr == received_end)
				{
					this->_header += std::string(received, bytes_read);
				}
				// If "\r\n\r\n" is found in received
				else
				{
					this->_header.append(received, ptr + 4);  // Include "\r\n\r\n"
					this->body.append(ptr + 4, received_end); // Skip "\r\n\r\n"
					this->client_read_state = ClientReadState::BODY;
					if (!_parseHeaders())
						return false;
				}
			}
		}
		else if (this->client_read_state == ClientReadState::BODY)
		{
			body += std::string(received, bytes_read);

			size_t server_to_cgi_pfds_index = fd_to_pfds_index.at(this->server_to_cgi_fd);
			std::cerr << "    Enabling server_to_cgi POLLOUT" << std::endl;
			pfds[server_to_cgi_pfds_index].events |= POLLOUT;
		}
		else
		{
			// Should be unreachable
			// TODO: Remove this before the evaluation
			assert(false);
		}

		// TODO: Replace this with Victor's parsed content length value
		// TODO: Move this block to be the first thing that happens below the "if (fd_type == FdType::CLIENT)",
		// TODO: because we want to set the read state to DONE as soon as possible for cleanliness
		if (this->body == "hello world\n")
		{
			std::cerr << "    Read the entire client's body:\n----------\n" << this->body << "\n----------" << std::endl;

			this->client_read_state = ClientReadState::DONE;
		}
	}
	else if (fd_type == FdType::CGI_TO_SERVER)
	{
		if (this->cgi_read_state == CGIReadState::READING_FROM_CGI)
		{
			response += std::string(received, bytes_read);
		}
		else
		{
			// Should be unreachable
			// TODO: Remove this before the evaluation
			assert(false);
		}
	}
	else
	{
		// Should be unreachable
		// TODO: Remove this before the evaluation
		assert(false);
	}

	return true;
}

/*	Private member functions */

// // TODO: Put in debug.cpp or smth?
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

int Client::_getFdFromFdType(FdType::FdType fd_type)
{
	if (fd_type == FdType::CLIENT)
	{
		return this->client_fd;
	}
	if (fd_type == FdType::CGI_TO_SERVER)
	{
		return this->cgi_to_server_fd;
	}

	// Should be unreachable
	// TODO: Remove this before the evaluation
	assert(false);
}

bool Client::_parseHeaders(void)
{
	std::vector<std::string> split;
	size_t pos;
	size_t start = 0;

	// Delete the trailing "\r\n" that separates the headers from the body
	this->_header.erase(this->_header.size() - 2);

	// std::cerr << "\"" << ToHex(this->_header) << "\"" << std::endl;

	while ((pos = this->_header.find("\r\n", start)) != std::string::npos)
	{
		split.push_back(this->_header.substr(start, pos - start));
		start = pos + 2;
	}

	if (!this->_parseStartLine(split[0]))
		return false;

	// TODO: Look what to do with HTTP_ thingy (See page 11 & 19 in cgi rfc)
	// Loop for every header
	for (size_t i = 1; i < split.size(); i++)
	{
		std::string line = split[i];

		// Assign key to everything before the ':' seperator
		pos = line.find(":");
		if (pos == std::string::npos)
			return false;
		std::string key = line.substr(0, pos);

		// Check if key is a duplicate
		if (this->header_map.find(key) != this->header_map.end())
			return false;

		// Capitalize letters and replace '-' with '_'
		for (size_t j = 0; j < key.size(); j++)
		{
			key[j] = toupper(key[j]);
			if (key[j] == '-')
				key[j] = '_';
		}

		// Skip all leading spaces and tabs before value
		pos++;
		while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
			pos++;

		// Assign value to everything after the whitespaces
		std::string value = line.substr(pos);

		// Erase all trailing spaces and tabs after value
		for (size_t j = 0; j < value.size(); j++)
		{
			if (value[j] == ' ' || value[j] == '\t')
			{
				value.erase(j);
				break;
			}
		}

		// Add key and value to the map
		this->header_map.insert(std::make_pair(key, value));
	}
	return true;
}

bool Client::_parseStartLine(std::string line)
{
	// Find and set the request method
	size_t request_method_end_pos = line.find(" ");
	if (request_method_end_pos == std::string::npos)
		return false;
	this->request_method = line.substr(0, request_method_end_pos);
	request_method_end_pos++;

	// Find and set the path
	size_t path_end_pos = line.find_last_of(" ");
	if (path_end_pos == std::string::npos || path_end_pos == request_method_end_pos - 1)
		return false;
	this->path = line.substr(request_method_end_pos, path_end_pos - request_method_end_pos);
	path_end_pos++;

	// Set and validate the protocol
	// // Validate "HTTP/"
	this->protocol = line.substr(path_end_pos);
	if (this->protocol.find("HTTP/", 0, 5) == std::string::npos)
		return false;

	// // Validate major version
	size_t i;
	for (i = 5; i < this->protocol.size(); i++)
	{
		if (this->protocol[i] < '0' || this->protocol[i] > '9')
			break;
	}

	// // Validate version seperator
	if (i == 5 || i == this->protocol.size() || this->protocol[i] != '.')
		return false;

	// // Validate minor version
	i++;
	size_t j = i;
	for (; i < this->protocol.size(); i++)
	{
		if (this->protocol[i] < '0' || this->protocol[i] > '9')
			break;
	}
	if (j == i || i != this->protocol.size())
		return false;
	return true;
}
