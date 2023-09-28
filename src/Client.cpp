#include "Client.hpp"

#include <algorithm>
#include <assert.h> // TODO: DELETE WHEN FINISHING PROJECT
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <poll.h>
#include <sstream>
#include <unistd.h>
#include <vector>

// TODO: REMOVE THIS FROM THIS FILE; IT'S ALREADY IN Server.cpp!
#define MAX_RECEIVED_LEN 100

/*	Orthodox Canonical Form */

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
	  cgi_pid(-1),
	  cgi_exit_status(-1),
	  _content_length(0),
	  _header(),
	  _is_chunked(false),
	  _chunked_remaining_content_length(0),
	  _chunked_body_buffer(),
	  _chunked_read_state(READING_CONTENT_LEN)
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
	  header_map(src.header_map),
	  body(src.body),
	  body_index(src.body_index),
	  response(src.response),
	  response_index(src.response_index),
	  client_fd(src.client_fd),
	  server_to_cgi_fd(src.server_to_cgi_fd),
	  cgi_to_server_fd(src.cgi_to_server_fd),
	  cgi_pid(src.cgi_pid),
	  cgi_exit_status(src.cgi_exit_status),
	  _content_length(src._content_length),
	  _header(src._header),
	  _is_chunked(src._is_chunked),
	  _chunked_remaining_content_length(src._chunked_remaining_content_length),
	  _chunked_body_buffer(src._chunked_body_buffer),
	  _chunked_read_state(src._chunked_read_state)
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
	this->header_map = src.header_map;
	this->body = src.body;
	this->body_index = src.body_index;
	this->response = src.response;
	this->response_index = src.response_index;
	this->client_fd = src.client_fd;
	this->server_to_cgi_fd = src.server_to_cgi_fd;
	this->cgi_to_server_fd = src.cgi_to_server_fd;
	this->cgi_pid = src.cgi_pid;
	this->cgi_exit_status = src.cgi_exit_status;
	this->_content_length = src._content_length;
	this->_header = src._header;
	this->_is_chunked = src._is_chunked;
	this->_chunked_body_buffer = src._chunked_body_buffer;
	this->_chunked_remaining_content_length = src._chunked_remaining_content_length;
	return *this;
}

/*	Static functions */

static bool hex_to_num(std::string &line, size_t &num)
{
	size_t i = 0;

	// Validate that line starts with a hexadecimal digit
	if (line[i] >= '0' && line[i] <= '9')
	{
		num += line[i] - '0';
		i++;
	}
	else if (line[i] >= 'A' && line[i] <= 'F')
	{
		num += line[i] - 'A' + 10;
		i++;
	}
	else if (line[i] >= 'a' && line[i] <= 'f')
	{
		num += line[i] - 'a' + 10;
		i++;
	}
	else
	{
		line.erase(0, i);
		return false;
	}
	// Convert rest of the possible numbers
	while (i < line.size())
	{
		if (line[i] >= '0' && line[i] <= '9')
		{
			num *= 16;
			num += line[i] - '0';
			i++;
		}
		else if (line[i] >= 'A' && line[i] <= 'F')
		{
			num *= 16;
			num += line[i] - 'A' + 10;
			i++;
		}
		else if (line[i] >= 'a' && line[i] <= 'f')
		{
			num *= 16;
			num += line[i] - 'a' + 10;
			i++;
		}
		else
		{
			line.erase(0, i);
			return true;
		}
	}
	line.erase(0, i);
	return true;
}

static bool strict_stoul(std::string &line, size_t &num)
{
	num = 0;
	for (size_t i = 0; i < line.size(); i++)
	{
		if (line[i] < '0' || line[i] > '9')
			return false;
		num *= 10;
		num += line[i] - '0';
	}
	return true;
}

/*	Public member functions */

// TODO: Put in debug.cpp or smth?
// #include <sstream>
// #include <iostream>
// #include <iomanip>
// static std::string to_hex(const std::string &s, bool upper_case = true)
// {
// 	std::stringstream ret;

// 	for (std::string::size_type i = 0; i < s.size(); ++i)
// 	{
// 		ret << std::hex << std::setfill('0') << std::setw(2) << (upper_case ? std::uppercase : std::nouppercase) << (int)s[i];
// 	}

// 	return ret.str();
// }

bool Client::appendReadString(char *received, ssize_t bytes_read)
{
	if (this->client_read_state == ClientReadState::HEADER)
	{
		this->_header.append(received, bytes_read);

		size_t found_index = this->_header.find("\r\n\r\n");
		if (found_index != std::string::npos)
		{
			std::string extra_body = std::string(this->_header.begin() + found_index + 4, this->_header.end());

			// Erase temporarily appended body bytes from _header
			this->_header.erase(this->_header.begin() + found_index + 2, this->_header.end());

			// Sets this->request_method
			if (!_parseHeaders())
				return false;

			std::cerr << "Request method: '" << this->request_method << "'" << std::endl;
			if (this->request_method == "POST")
			{
				// If no content_length was given
				if (this->_content_length == 0)
				{
					this->client_read_state = ClientReadState::DONE;
				}
				// If content_length was given
				else
				{
					this->client_read_state = ClientReadState::BODY;

					// Add body bytes to _body
					if (extra_body.size() > 0 && !this->_parseBodyAppend(extra_body))
					{
						return false;
					}
				}
			}
			else
			{
				// GET and DELETE requests have no body
				this->client_read_state = ClientReadState::DONE;

				return true;
			}
		}
	}
	else if (this->client_read_state == ClientReadState::BODY)
	{
		if (!this->_parseBodyAppend(std::string(received, received + bytes_read)))
			return false;
	}
	else
	{
		// We keep reading regardless of whether we saw the end of the body,
		// so we can still read the EOF any disconnected client sent
		return true;
	}

	return true;
}

// This method assumes the client has read all bytes from the reaped cgi's stdout
void Client::prependResponseHeader()
{
	// TODO: Don't hardcode the header
	// TODO: Use cgi_exit_status

	std::stringstream len_ss;
	len_ss << this->response.size();
	// TODO: Use _replace_all(str, "\n", "\r\n"):
	// cgi_rfc3875.pdf: "The server MUST translate the header data from the CGI header syntax to the HTTP header syntax if these differ."

	this->response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + len_ss.str() + "\r\n\r\n" + this->response;
}

/*	Private member functions */

bool Client::_parseHeaders(void)
{
	std::vector<std::string> split;
	size_t pos;
	size_t start = 0;

	// Delete the trailing "\r\n" that separates the headers from the body
	this->_header.erase(this->_header.size() - 2);

	// std::cerr << "\"" << to_hex(this->_header) << "\"" << std::endl;

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
		if (this->header_map.find(key) != this->header_map.end())
			return false;
		this->header_map.emplace(key, value);
		if (key == "TRANSFER_ENCODING" && value == "chunked")
		{
			// A sender MUST NOT send a Content-Length header field in any message that contains a Transfer-Encoding header field. (http1.1 rfc 6.2)
			if (this->header_map.find("CONTENT_LENGTH") != this->header_map.end())
				return false;
			this->_is_chunked = true;
		}
		else if (key == "CONTENT_LENGTH")
		{
			// A sender MUST NOT send a Content-Length header field in any message that contains a Transfer-Encoding header field. (http1.1 rfc 6.2)
			if (this->_is_chunked)
				return false;

			// Check if value to content_length is a valid positive number
			if (!strict_stoul(value, this->_content_length))
				return false;
		}
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

bool Client::_parseBodyAppend(const std::string &extra_body)
{
	if (this->_is_chunked)
	{
		std::cerr << "Parsing chunked body substring" << std::endl;
		this->_chunked_body_buffer.append(extra_body);
		while (true)
		{
			if (this->_chunked_read_state == READING_CONTENT_LEN)
			{
				// Check if current buffer has anything else than a hexadecimal character
				if (this->_chunked_body_buffer.find_first_not_of("1234567890abcdefABCDEF") == std::string::npos)
					break;

				// Returns false if start of buffer isn't hexadecimal
				if (!hex_to_num(this->_chunked_body_buffer, this->_chunked_remaining_content_length))
					return false;

				this->_content_length += this->_chunked_remaining_content_length; // TODO: This isn't needed but is nice to just update it as appropriate (Maybe want to delete because it isn't needed)
				this->_chunked_read_state = READING_CONTENT_LEN_ENDLINE;
			}
			if (this->_chunked_read_state == READING_BODY)
			{
				// If not all of extra_body should fit into the body
				if (this->_chunked_body_buffer.size() >= this->_chunked_remaining_content_length)
				{
					this->body.append(this->_chunked_body_buffer, 0, this->_chunked_remaining_content_length);
					this->_chunked_body_buffer.erase(0, this->_chunked_remaining_content_length);
					this->_chunked_remaining_content_length = 0;
					this->_chunked_read_state = READING_BODY_ENDLINE;
				}
				// If all of extra_body should fit into the body
				else
				{
					this->body.append(this->_chunked_body_buffer);
					this->_chunked_remaining_content_length -= this->_chunked_body_buffer.size();
					this->_chunked_body_buffer.erase();
					break;
				}
			}
			// If state is any _ENDLINE state
			if (this->_chunked_read_state == READING_CONTENT_LEN_ENDLINE || this->_chunked_read_state == READING_BODY_ENDLINE)
			{
				if (this->_chunked_body_buffer.size() < 2)
					break;

				// If buffer starts with "\r\n" (as expected)
				if (this->_chunked_body_buffer[0] == '\r' && this->_chunked_body_buffer[1] == '\n')
				{
					this->_chunked_body_buffer.erase(0, 2);

					// If at the end of chunked requests
					if (this->_chunked_remaining_content_length == 0 && this->_chunked_read_state == READING_CONTENT_LEN_ENDLINE)
					{
						this->client_read_state = ClientReadState::DONE;
					}
					else if (this->_chunked_read_state == READING_CONTENT_LEN_ENDLINE)
					{
						this->_chunked_read_state = READING_BODY;
					}
					else if (this->_chunked_read_state == READING_BODY_ENDLINE)
					{
						this->_chunked_read_state = READING_CONTENT_LEN;
					}
					// Should never reach
					else
					{
						assert(false); // TODO: Delete before submitting for eval
					}
				}
				// If buffer doesn't start with "\r\n" (This means client sent incorrectly formatted data)
				else
				{
					return false;
				}
			}
		}
	}
	// If request is not chunked
	else
	{
		std::cerr << "Parsing non-chunked body substring" << std::endl;
		// If not all of extra_body should fit into the body
		if (this->body.size() + extra_body.size() >= this->_content_length)
		{
			size_t needed = this->_content_length - this->body.size();
			this->body.append(extra_body.begin(), extra_body.begin() + needed);
			this->client_read_state = ClientReadState::DONE;
		}
		// If all of extra_body should fit into the body
		else
		{
			this->body.append(extra_body);
		}
	}
	return true;
}

void Client::_generateEnv()
{
	// TODO: Decide what variable to give the CGI, and if we have to put HTTP_ in front of all the keys
	// See page 19, section 4.1.18 in CGI RFC 3875
}

// std::string Client::_replace_all(std::string input, const std::string& needle, const std::string& replacement)
// {
// 	size_t start_pos = 0;
// 	while((start_pos = input.find(needle, start_pos)) != std::string::npos)
// 	{
// 		input.replace(start_pos, needle.size(), replacement);
// 		start_pos += replacement.size(); // Handles case where 'replacement' is a substring of 'needle'
// 	}
// 	return input;
// }
