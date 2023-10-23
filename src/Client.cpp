#include "Client.hpp"

#include "Utils.hpp"

#include <algorithm>
#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <poll.h>
#include <sstream>
#include <unistd.h>
#include <vector>

// TODO: REMOVE THIS FROM THIS FILE; IT'S ALREADY IN Server.cpp!
#define MAX_RECEIVED_LEN 100

const char *Client::status_text_table[] = {
	[Status::OK] = "OK",
	[Status::BAD_REQUEST] = "Bad Request",
	[Status::FORBIDDEN] = "Forbidden",
	[Status::NOT_FOUND] = "Not Found",
	[Status::METHOD_NOT_ALLOWED] = "Method Not Allowed",
};

/*	Orthodox Canonical Form */

Client::Client(int client_fd)
	: status(Status::OK),
	  client_read_state(ClientReadState::HEADER),
	  cgi_write_state(CGIWriteState::NOT_WRITING),
	  cgi_read_state(CGIReadState::NOT_READING),
	  client_write_state(ClientWriteState::NOT_WRITING),
	  request_method(),
	  request_target(),
	  protocol(),
	  headers(),
	  port(-1),
	  body(),
	  body_index(),
	  response(),
	  response_index(),
	  client_fd(client_fd),
	  server_to_cgi_fd(-1),
	  cgi_to_server_fd(-1),
	  cgi_pid(-1),
	  cgi_exit_status(-1),
	  _content_length(),
	  _header(),
	  _is_chunked(),
	  _chunked_remaining_content_length(),
	  _chunked_body_buffer(),
	  _chunked_read_state(READING_CONTENT_LEN)
{
}

Client::Client(Client const &src)
	: status(src.status),
	  client_read_state(src.client_read_state),
	  cgi_write_state(src.cgi_write_state),
	  cgi_read_state(src.cgi_read_state),
	  client_write_state(src.client_write_state),
	  request_method(src.request_method),
	  request_target(src.request_target),
	  protocol(src.protocol),
	  headers(src.headers),
	  port(src.port),
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
	this->status = src.status;
	this->client_read_state = src.client_read_state;
	this->cgi_write_state = src.cgi_write_state;
	this->cgi_read_state = src.cgi_read_state;
	this->client_write_state = src.client_write_state;
	this->request_method = src.request_method;
	this->request_target = src.request_target;
	this->protocol = src.protocol;
	this->headers = src.headers;
	this->port = src.port;
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

/*	Public member functions */

void Client::appendReadString(char *received, ssize_t bytes_read)
{
	if (this->client_read_state == ClientReadState::HEADER)
	{
		this->_header.append(received, bytes_read);

		// Return if the header hasn't been fully read yet
		size_t found_index = this->_header.find("\r\n\r\n");
		if (found_index == std::string::npos)
			return;

		std::string extra_body = std::string(this->_header.begin() + found_index + 4, this->_header.end());

		// Erase temporarily appended body bytes from _header
		this->_header.erase(this->_header.begin() + found_index + 2, this->_header.end());

		const auto header_lines = this->_getHeaderLines();

		this->_parseRequestLine(header_lines[0]);
		if (!this->_isValidRequestLine()) throw ClientException(Status::BAD_REQUEST);

		// Resolves "/.." to "/" to prevent escaping the public directory
		this->request_target = std::filesystem::weakly_canonical(this->request_target);

		this->_fillHeaders(header_lines);

		this->_useHeaders();

		// Only POST requests have a body
		if (this->request_method != "POST")
		{
			this->client_read_state = ClientReadState::DONE;
			return;
		}

		this->client_read_state = ClientReadState::BODY;

		// Add body bytes to _body
		if (extra_body.size() > 0)
			this->_parseBodyAppend(extra_body);
	}
	else if (this->client_read_state == ClientReadState::BODY)
		this->_parseBodyAppend(std::string(received, received + bytes_read));
	else if (this->client_read_state == ClientReadState::DONE)
	{
		// We keep reading regardless of whether we saw the end of the body,
		// so we can still read the EOF any disconnected client sent
	}
}

void Client::respondWithFile(const std::string &path)
{
	assert(this->response.empty());

	std::ifstream file(path);
	std::stringstream file_body;
	file_body << file.rdbuf();
	this->response = file_body.str();

	this->prependResponseHeader();
}

void Client::respondWithDirectoryListing(const std::string &path)
{
	// TODO: Write
	(void)path;
	assert(false);
}

void Client::respondWithRedirect(const std::string &path)
{
	// TODO: Write
	(void)path;
	assert(false);
}

void Client::respondWithCreateFile(const std::string &path)
{
	// TODO: Write
	(void)path;
	assert(false);
}

void Client::respondWithDeleteFile(const std::string &path)
{
	// TODO: Write
	(void)path;
	assert(false);
}

void Client::prependResponseHeader(void)
{
	std::string response_body = this->response;
	this->response = "";

	_addStatusLine();

	// TODO: Do we ever want to send anything besides the status line when status is not OK?
	if (this->status == Status::OK)
	{
		// TODO: Unhardcode the Content-Type?
		this->response += "Content-Type: text/plain\r\n";

		// TODO: Use cgi_exit_status
		std::stringstream len_ss;
		len_ss << response_body.size();
		// TODO: Use _replaceAll(str, "\n", "\r\n"):
		// cgi_rfc3875.pdf: "The server MUST translate the header data from the CGI header syntax to the HTTP header syntax if these differ."

		this->response +=
			"Content-Length: "
			+ len_ss.str()
			+ "\r\n";
	}

	this->response +=
		"\r\n"
		+ response_body;
}

/*	Private member functions */

std::vector<std::string> Client::_getHeaderLines(void)
{
	std::vector<std::string> header_lines;

	// Delete the trailing "\r\n" that separates the headers from the body
	this->_header.erase(this->_header.size() - 2);

	size_t start = 0;
	while (true)
	{
		size_t i = this->_header.find("\r\n", start);
		if (i == std::string::npos)
			break;
		header_lines.push_back(this->_header.substr(start, i - start));
		start = i + 2;
	}

	return header_lines;
}

void Client::_parseRequestLine(const std::string &line)
{
	// Find and set the request method
	size_t request_method_end_pos = line.find(" ");
	if (request_method_end_pos == std::string::npos) throw ClientException(Status::BAD_REQUEST);
	this->request_method = line.substr(0, request_method_end_pos);
	request_method_end_pos++;
	std::cerr << "    Request method: '" << this->request_method << "'" << std::endl;

	// Find and set the request target
	size_t path_end_pos = line.find_last_of(" ");
	if (path_end_pos == std::string::npos || path_end_pos == request_method_end_pos - 1) throw ClientException(Status::BAD_REQUEST);
	this->request_target = line.substr(request_method_end_pos, path_end_pos - request_method_end_pos);
	path_end_pos++;
	std::cerr << "    Request target: '" << this->request_target << "'" << std::endl;

	// Set the protocol
	this->protocol = line.substr(path_end_pos);
	std::cerr << "    HTTP version: '" << this->protocol << "'" << std::endl;
}

bool Client::_isValidRequestLine(void)
{
	return this->_isValidRequestMethod() && this->_isValidRequestTarget() && this->_isValidProtocol();
}

bool Client::_isValidRequestMethod(void)
{
	return this->request_method == "GET" || this->request_method == "POST" || this->request_method == "DELETE";
}

bool Client::_isValidRequestTarget(void)
{
	return this->request_target[0] == '/';
}

// See RFC 9112 section 2.3
bool Client::_isValidProtocol(void)
{
	if (this->protocol.size() < sizeof("HTTP/0.0") - 1)
		return false;
	if (!Utils::startsWith(this->protocol, "HTTP/"))
		return false;
	if (this->protocol[5] < '0' || this->protocol[5] > '9')
		return false;
	if (this->protocol[6] != '.')
		return false;
	if (this->protocol[7] < '0' || this->protocol[7] > '9')
		return false;
	return true;
}

void Client::_fillHeaders(const std::vector<std::string> &header_lines)
{
	for (size_t line_index = 1; line_index < header_lines.size(); line_index++)
	{
		const std::string &line = header_lines.at(line_index);

		// Assign key to everything before the ':' seperator
		size_t i = line.find(":");
		if (i == std::string::npos) throw ClientException(Status::BAD_REQUEST);
		std::string key = line.substr(0, i);

		// Capitalize letters and replace '-' with '_'
		for (size_t j = 0; j < key.size(); j++)
		{
			key[j] = toupper(key[j]);
			if (key[j] == '-')
				key[j] = '_';
		}

		// Skip all leading spaces and tabs before value
		i++;
		while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
			i++;

		// Assign value to everything after the whitespaces
		std::string value = line.substr(i);

		// Erase all trailing spaces and tabs after value
		for (size_t j = 0; j < value.size(); j++)
		{
			if (value[j] == ' ' || value[j] == '\t')
			{
				value.erase(j);
				break;
			}
		}

		// Check if key is a duplicate
		if (this->headers.find(key) != this->headers.end()) throw ClientException(Status::BAD_REQUEST);

		// Add key and value to the map
		this->headers.emplace(key, value);
		std::cerr << "    Header key: '" << key << "', value: '" << value << "'" << std::endl;
	}
}

void Client::_useHeaders(void)
{
	const auto &transfer_encoding_it = this->headers.find("TRANSFER_ENCODING");
	if (transfer_encoding_it != this->headers.end() && transfer_encoding_it->second == "chunked")
	{
		this->_is_chunked = true;
	}

	const auto &content_length_it = this->headers.find("CONTENT_LENGTH");
	if (content_length_it != this->headers.end())
	{
		// A sender MUST NOT send a Content-Length header field in any message that contains a Transfer-Encoding header field. (http1.1 rfc 6.2)
		if (this->_is_chunked) throw ClientException(Status::BAD_REQUEST);

		if (!Utils::parseNumber(content_length_it->second, this->_content_length, std::numeric_limits<size_t>::max())) throw ClientException(Status::BAD_REQUEST);

		std::cerr << "    Content length: " << this->_content_length << std::endl;
	}

	const auto &host_it = this->headers.find("HOST");
	if (host_it == this->headers.end())
	{
		// "A client MUST send a Host header field" - HTTP/1.1 RFC 9112, section 3.2 Request Target
		throw Client::ClientException(Status::BAD_REQUEST);
	}
	else
	{
		const std::string &host = host_it->second;

		size_t colon_index = host.find(":");
		if (colon_index == host.npos)
		{
			// "If no port is included, the default port for the service requested is implied"
			// Source: https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Host
			this->port = 80;
		}
		else
		{
			// If there is no port after the colon
			if (colon_index == host.size() - 1) throw Client::ClientException(Status::BAD_REQUEST);

			std::string port_str = host.substr(colon_index + 1);
			if (!Utils::parseNumber(port_str, this->port, static_cast<uint16_t>(65535))) throw Client::ClientException(Status::BAD_REQUEST);
		}
	}
}

void Client::_parseBodyAppend(const std::string &extra_body)
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

				_hexToNum(this->_chunked_body_buffer, this->_chunked_remaining_content_length);

				this->_content_length += this->_chunked_remaining_content_length; // TODO: This isn't needed but it's nice to just update it as appropriate (Maybe want to delete because it isn't needed)
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
				else throw ClientException(Status::BAD_REQUEST);
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
			this->body.append(extra_body);
	}
}

// Consumes characters from the start of line, using them to increase num
void Client::_hexToNum(std::string &line, size_t &num)
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
		throw ClientException(Status::BAD_REQUEST);
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
			return;
		}
	}
	line.erase(0, i);
}

void Client::_addStatusLine(void)
{
	this->response +=
		this->protocol
		+ " "
		+ std::to_string(this->status)
		+ " "
		+ status_text_table[this->status]
		+ "\r\n";
}

void Client::_generateEnv(void)
{
	// TODO: Decide what variable to give the CGI, and if we have to put HTTP_ in front of all the keys
	// See page 19, section 4.1.18 in CGI RFC 3875

	// TODO: Do this:
	// key = "HTTP_" + key;
}

// TODO: Remove?
// std::string Client::_replaceAll(std::string input, const std::string& needle, const std::string& replacement)
// {
// 	size_t start_pos = 0;
// 	while((start_pos = input.find(needle, start_pos)) != std::string::npos)
// 	{
// 		input.replace(start_pos, needle.size(), replacement);
// 		start_pos += replacement.size(); // Handles case where 'replacement' is a substring of 'needle'
// 	}
// 	return input;
// }
