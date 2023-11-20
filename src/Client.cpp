#include "Client.hpp"

#include "Logger.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <poll.h>
#include <sstream>
#include <vector>

namespace L = Logger;

const char *Client::reason_phrase_table[] = {
	[Status::OK] = "OK",
	[Status::MOVED_PERMANENTLY] = "Moved Permanently",
	[Status::MOVED_TEMPORARILY] = "Moved Temporarily",
	[Status::BAD_REQUEST] = "Bad Request",
	[Status::FORBIDDEN] = "Forbidden",
	[Status::NOT_FOUND] = "Not Found",
	[Status::METHOD_NOT_ALLOWED] = "Method Not Allowed",
	[Status::REQUEST_ENTITY_TOO_LARGE] = "Request Entity Too Large",
	[Status::INTERNAL_SERVER_ERROR] = "Internal Server Error",
};

class Config;

Client::Client(int client_fd, int server_fd, const std::string &server_port, const size_t &client_max_body_size)
	: server_fd(server_fd),
	  status(Status::OK),
	  client_to_server_state(ClientToServerState::HEADER),
	  server_to_cgi_state(ServerToCGIState::NOT_WRITING),
	  cgi_to_server_state(CGIToServerState::NOT_READING),
	  server_to_client_state(ServerToClientState::NOT_WRITING),
	  request_method(RequestMethod::NONE),
	  request_target(),
	  protocol("HTTP/1.1"),
	  headers(),
	  body(),
	  body_index(),
	  client_max_body_size(client_max_body_size),
	  response(),
	  response_index(),
	  client_fd(client_fd),
	  server_to_cgi_fd(-1),
	  cgi_to_server_fd(-1),
	  cgi_pid(-1),
	  cgi_exit_status(-1),
	  redirect(),
	  server_name(),
	  server_port(server_port),
	  content_type(),
	  content_length(),
	  _custom_reason_phrase(),
	  _response_content_type("application/octet-stream"),
	  _header(),
	  _is_chunked(),
	  _chunked_remaining_content_length(),
	  _chunked_body_buffer(),
	  _chunked_read_state(ChunkedReadState::READING_CONTENT_LEN)
{
}

Client::Client(Client const &src)
	: server_fd(src.server_fd),
	  status(src.status),
	  client_to_server_state(src.client_to_server_state),
	  server_to_cgi_state(src.server_to_cgi_state),
	  cgi_to_server_state(src.cgi_to_server_state),
	  server_to_client_state(src.server_to_client_state),
	  request_method(src.request_method),
	  request_target(src.request_target),
	  protocol(src.protocol),
	  headers(src.headers),
	  body(src.body),
	  body_index(src.body_index),
	  client_max_body_size(src.client_max_body_size),
	  response(src.response),
	  response_index(src.response_index),
	  client_fd(src.client_fd),
	  server_to_cgi_fd(src.server_to_cgi_fd),
	  cgi_to_server_fd(src.cgi_to_server_fd),
	  cgi_pid(src.cgi_pid),
	  cgi_exit_status(src.cgi_exit_status),
	  redirect(src.redirect),
	  server_name(src.server_name),
	  server_port(src.server_port),
	  content_type(src.content_type),
	  content_length(src.content_length),
	  _custom_reason_phrase(src._custom_reason_phrase),
	  _response_content_type(src._response_content_type),
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
	this->server_fd = src.server_fd;
	this->status = src.status;
	this->client_to_server_state = src.client_to_server_state;
	this->server_to_cgi_state = src.server_to_cgi_state;
	this->cgi_to_server_state = src.cgi_to_server_state;
	this->server_to_client_state = src.server_to_client_state;
	this->request_method = src.request_method;
	this->request_target = src.request_target;
	this->protocol = src.protocol;
	this->headers = src.headers;
	this->body = src.body;
	this->body_index = src.body_index;
	this->client_max_body_size = src.client_max_body_size;
	this->response = src.response;
	this->response_index = src.response_index;
	this->client_fd = src.client_fd;
	this->server_to_cgi_fd = src.server_to_cgi_fd;
	this->cgi_to_server_fd = src.cgi_to_server_fd;
	this->cgi_pid = src.cgi_pid;
	this->cgi_exit_status = src.cgi_exit_status;
	this->redirect = src.redirect;
	this->server_name = src.server_name;
	this->server_port = src.server_port;
	this->content_type = src.content_type;
	this->content_length = src.content_length;
	this->_custom_reason_phrase = src._custom_reason_phrase;
	this->_response_content_type = src._response_content_type;
	this->_header = src._header;
	this->_is_chunked = src._is_chunked;
	this->_chunked_body_buffer = src._chunked_body_buffer;
	this->_chunked_remaining_content_length = src._chunked_remaining_content_length;
	return *this;
}

void Client::appendReadString(char *received, ssize_t bytes_read)
{
	if (this->client_to_server_state == ClientToServerState::HEADER)
	{
		this->_header.append(received, static_cast<size_t>(bytes_read));

		// Return if the header hasn't been fully read yet
		size_t separator_index = this->_header.find("\r\n\r\n");
		if (separator_index == std::string::npos)
			return;

		// TODO: Is the .end() implicit already?
		std::string extra_body = std::string(this->_header.begin() + separator_index + 4, this->_header.end());

		// Erase temporarily appended body bytes from _header
		this->_header.erase(this->_header.begin() + separator_index + 2, this->_header.end());

		std::vector<std::string> header_lines = this->_getHeaderLines();

		std::string request_line = header_lines.at(0);
		// L::debug("request_line: " + request_line);
		header_lines.erase(header_lines.begin());

		this->_parseRequestLine(request_line);

		if (!this->_fillHeaders(header_lines, this->headers)) throw ClientException(Status::BAD_REQUEST);

		this->_useHeaders();

		// TODO: Can a GET or a DELETE have a body?
		if (this->request_method != RequestMethod::POST || this->content_length == 0)
		{
			this->client_to_server_state = ClientToServerState::DONE;
			this->server_to_cgi_state = ServerToCGIState::DONE;
			return;
		}

		this->client_to_server_state = ClientToServerState::BODY;

		// Add body bytes to _body
		if (extra_body.size() > 0)
			this->_parseBodyAppend(extra_body);
	}
	else if (this->client_to_server_state == ClientToServerState::BODY)
		this->_parseBodyAppend(std::string(received, received + bytes_read));
	else if (this->client_to_server_state == ClientToServerState::DONE)
	{
		// We keep reading regardless of whether we saw the end of the body,
		// so we can still read the EOF any disconnected client sent
	}
}

void Client::respondWithError(const std::unordered_map<Status::Status, std::string> &error_pages)
{
	bool opened_file = false;

	const auto &error_page_it = error_pages.find(this->status);

	if (error_page_it != error_pages.end())
	{
		std::ifstream file(error_page_it->second);
		if (file.is_open())
		{
			opened_file = true;

			std::stringstream file_body;
			file_body << file.rdbuf();

			this->response = file_body.str();
		}
	}

	if (!opened_file)
	{
		std::string title = std::to_string(this->status) + " " + reason_phrase_table[this->status];

		this->response =
			"<html>\n"
			"<head><title>" + title + "</title></head>\n"
			"<body>\n"
			"<center><h1>" + title + "</h1></center>\n"
			"<hr><center>webserv</center>\n"
			"</body>\n"
			"</html>\n";
	}

	this->response_index = 0;

	this->_response_content_type = "text/html";

	this->prependResponseHeader();
}

void Client::respondWithFile(const std::string &path)
{
	assert(this->response.empty());

	std::ifstream file(path);
	if (!file.is_open())
	{
		throw ClientException(Status::NOT_FOUND);
	}

	std::stringstream file_body;
	file_body << file.rdbuf();

	this->response = file_body.str();

	static std::unordered_map<std::string, std::string> extension_to_content_type = {
		{".c", "text/plain"},
		{".cpp", "text/plain"},
		{".h", "text/plain"},
		{".hpp", "text/plain"},
		{".log", "text/plain"},
		{".md", "text/plain"},
		{".mk", "text/plain"},
		{".py", "text/plain"},
		{".tpp", "text/plain"},
		{".txt", "text/plain"},

		{".html", "text/html"},
		{".mp3", "audio/mpeg"},
		{".png", "image/png"},
		{".pdf", "application/pdf"},
		{".gif", "image/gif"},
		{".json", "application/json"},
		{".mp4", "video/mp4"},
	};

	auto it = extension_to_content_type.find(_getFileExtension(path));
	if (it != extension_to_content_type.end())
	{
		this->_response_content_type = it->second;
	}

	// L::info(std::string("    File name: '") + _getFileName(path) + "'");
	if (_getFileName(path) == "Makefile")
	{
		this->_response_content_type = "text/plain";
	}

	this->prependResponseHeader();
}

void Client::respondWithDirectoryListing(const std::string &path)
{
	assert(this->response.empty());

	this->response =
		"<!DOCTYPE html>\n"
		"<html>\n"
    	"	<meta charset=\"UTF-8\">\n"
		"	<head>\n"
		"		<title>Index of " + this->request_target + "</title>\n"
		"		<style>\n"
		"			body {\n"
		"				background-color: gray;\n"
		"				font-size: large;\n"
		"			}\n"
		"			a {\n"
		"				padding-left: 2px;\n"
		"				margin-left: 4px;\n"
		"			}\n"
		"			button {\n"
		"				background-color: gray;\n"
		"				padding-left: 3px;\n"
		"				padding-right: 3px;\n"
		"				border: 0px;\n"
		"				margin: 2px;\n"
		"				cursor: pointer;\n"
		"			}\n"
		"			hr {\n"
		"				border-color: black;\n"
		"				background-color: black;\n"
		"				color: black;\n"
		"			}\n"
		"		</style>\n"
		"	</head>\n"
		"	<body>\n"
		"		<h1>Index of " + this->request_target + "</h1>\n"
		"		<hr>\n"
		"		<pre>\n";

	for (const std::string &directory_name : _getSortedEntryNames(path, true))
	{
		this->response +=
			"<a href=\""
			+ directory_name
			+ "\">"
			+ directory_name
			+ "</a>\n";
	}

	for (const std::string &non_directory_name : _getSortedEntryNames(path, false))
	{
		this->response +=
			"<a href=\""
			+ non_directory_name
			+ "\">"
			+ non_directory_name
			+ "</a>\n";
	}

	this->response +=
		"		</pre>\n"
		"		<hr>\n"
		"	</body>\n"
		"</html>\n";

	this->_response_content_type = "text/html";

	this->prependResponseHeader();
}

void Client::respondWithCreateFile(const std::string &path)
{
	assert(this->response.empty());

	std::ofstream outfile;

	outfile.open(path, std::fstream::in);

	if (outfile)
	{
		L::debug("    File already exists");
		throw ClientException(Status::BAD_REQUEST);
	}

	L::debug("    Creating file");
	outfile.open(path, std::fstream::out);

	if (!outfile)
	{
		L::debug("    Couldn't create file");
		throw ClientException(Status::BAD_REQUEST);
	}

	// TODO: Is it necessary to flush manually before the ofstream's destructor gets called?
	// TODO: Limit the body size somewhere before this point is even reached
	outfile << this->body;

	this->prependResponseHeader();
}

void Client::respondWithDeleteFile(const std::string &path)
{
	assert(this->response.empty());

	if (!std::filesystem::remove(path))
	{
		throw ClientException(Status::NOT_FOUND);
	}

	this->prependResponseHeader();
}

void Client::prependResponseHeader(void)
{
	L::info(std::string("    In prependResponseHeader()"));

	L::debug("this->status: " + std::to_string(this->status));

	std::string response_body = this->response;
	this->response = "";

	std::string reason_phrase = this->_custom_reason_phrase.empty() ? reason_phrase_table[this->status] : this->_custom_reason_phrase;

	this->response +=
		this->protocol
		+ " "
		+ std::to_string(this->status)
		+ " "
		+ reason_phrase
		+ "\r\n";

	_addResponseHeader("Content-Type", this->_response_content_type);

	// TODO: Use cgi_exit_status
	std::stringstream content_length_ss;
	content_length_ss << response_body.length();

	// TODO: Use _replaceAll(str, "\n", "\r\n"):
	// cgi_rfc3875.pdf: "The server MUST translate the header data from the CGI header syntax to the HTTP header syntax if these differ."

	_addResponseHeader("Content-Length", content_length_ss.str());

	if (this->status == Status::MOVED_PERMANENTLY)
	{
		this->_addResponseHeader("Location", "http://" + this->server_name + ":" + this->server_port + this->request_target + "/");
	}
	else if (this->status == Status::MOVED_TEMPORARILY)
	{
		assert(!this->redirect.empty());
		this->_addResponseHeader("Location", this->redirect);
	}

	// TODO: Add?
	// this->_addResponseHeader("Connection", "keep-alive");

	this->response +=
		"\r\n"
		+ response_body;
}

// See CGI RFC 3875 section 6.2.1. Document Response
void Client::extractCGIDocumentResponseHeaders(void)
{
	L::info(std::string("    In extractCGIDocumentResponseHeaders()"));
	// this->_header.append(received, bytes_read);

	// We don't send the CGI headers on 500 Internal Server Error
	if (this->cgi_exit_status != 0)
	{
		return;
	}

	size_t separator_index = this->response.find("\n\n");
	if (separator_index == std::string::npos)
	{
		this->status = Status::INTERNAL_SERVER_ERROR;
		return;
	}

	std::string cgi_header = std::string(this->response.begin(), this->response.begin() + separator_index + 1);
	// L::debug("cgi_header is '" + cgi_header + "'");

	// Erase the CGI header from this->response
	// L::debug("separator_index is '" + std::to_string(separator_index) + "'");
	this->response.erase(0, separator_index + 2);
	// L::debug("this->response is '" + this->response + "'");

	std::vector<std::string> cgi_header_lines;

	size_t start = 0;
	while (true)
	{
		size_t i = cgi_header.find("\n", start);
		if (i == std::string::npos)
			break;
		std::string header_line = cgi_header.substr(start, i - start);
		cgi_header_lines.push_back(header_line);
		// L::debug("Pushed header line '" + header_line + "'");
		start = i + 1;
	}

	std::unordered_map<std::string, std::string> cgi_headers;

	if (!this->_fillHeaders(cgi_header_lines, cgi_headers))
	{
		this->status = Status::INTERNAL_SERVER_ERROR;
		return;
	}

	if (cgi_headers.contains("CONTENT_TYPE"))
	{
		this->_response_content_type = cgi_headers.at("CONTENT_TYPE");
	}
	else
	{
		this->status = Status::INTERNAL_SERVER_ERROR;
		return;
	}

	if (cgi_headers.contains("STATUS"))
	{
		// See CGI RFC 3875 section 6.3.3. Status
		std::string status_string = cgi_headers.at("STATUS");
		// L::debug("status_string: " + status_string);

		size_t space_index = status_string.find(' ');
		// L::debug("space_index: " + std::to_string(space_index));
		if (space_index == std::string::npos)
		{
			this->status = Status::INTERNAL_SERVER_ERROR;
			return;
		}

		std::string status_code_string = status_string.substr(0, space_index);
		// L::debug("status_code_string: '" + status_code_string + "'");

		std::string reason_phrase = status_string.substr(space_index + 1);
		// L::debug("reason_phrase: '" + reason_phrase + "'");

		int status_code;
		if (!Utils::parseNumber(status_code_string, status_code, 999))
		{
			this->status = Status::INTERNAL_SERVER_ERROR;
			return;
		}

		this->status = Status::Status(status_code);
		this->_custom_reason_phrase = reason_phrase;
	}

	// TODO: What to do with headers that prependRespondHeader() doesn't expect?
}

std::string Client::getRequestMethodString(void) const
{
	switch (this->request_method)
	{
		case RequestMethod::NONE:
			assert(false);
		case RequestMethod::GET:
			return "GET";
			break;
		case RequestMethod::POST:
			return "POST";
			break;
		case RequestMethod::DELETE:
			return "DELETE";
			break;
	}
}

std::vector<std::string> Client::_getHeaderLines(void)
{
	std::vector<std::string> header_lines;

	// L::debug("this->_header is '" + this->_header + "'");

	size_t start = 0;
	while (true)
	{
		size_t i = this->_header.find("\r\n", start);
		if (i == std::string::npos)
			break;
		std::string header_line = this->_header.substr(start, i - start);
		header_lines.push_back(header_line);
		// L::debug("Pushed header line '" + header_line + "'");
		start = i + 2;
	}

	return header_lines;
}

void Client::_parseRequestLine(const std::string &line)
{
	// Find and set the request method
	size_t request_method_end_pos = line.find(" ");
	if (request_method_end_pos == std::string::npos) throw ClientException(Status::BAD_REQUEST);

	std::string request_method_string = line.substr(0, request_method_end_pos);
	this->request_method = this->_getRequestMethodEnumFromString(request_method_string);
	request_method_end_pos++;
	L::info(std::string("    Request method: '") + this->getRequestMethodString() + "'");

	// Find and set the request target
	size_t path_end_pos = line.find_last_of(" ");
	if (path_end_pos == std::string::npos || path_end_pos == request_method_end_pos - 1) throw ClientException(Status::BAD_REQUEST);

	this->request_target = line.substr(request_method_end_pos, path_end_pos - request_method_end_pos);
	if (this->request_target.empty() || this->request_target.at(0) != '/') throw ClientException(Status::BAD_REQUEST);
	L::info(std::string("    Request target: '") + this->request_target + "'");

	path_end_pos++;

	// Set the protocol
	this->protocol = line.substr(path_end_pos);
	if (!this->_isValidProtocol()) throw ClientException(Status::BAD_REQUEST);
	L::info(std::string("    Protocol: '") + this->protocol + "'");
}

Client::RequestMethod Client::_getRequestMethodEnumFromString(const std::string &request_method_string) const
{
	if (request_method_string == "GET")
	{
		return RequestMethod::GET;
	}
	else if (request_method_string == "POST")
	{
		return RequestMethod::POST;
	}
	else if (request_method_string == "DELETE")
	{
		return RequestMethod::DELETE;
	}
	throw ClientException(Status::METHOD_NOT_ALLOWED);
}

// See RFC 9112 section 2.3
bool Client::_isValidProtocol(void)
{
	return (this->protocol.size() == std::string("HTTP/0.0").size())
		&& (Utils::startsWith(this->protocol, "HTTP/"))
		&& (this->protocol[5] >= '0' && this->protocol[5] <= '9')
		&& (this->protocol[6] == '.')
		&& (this->protocol[7] >= '0' || this->protocol[7] <= '9');
}

bool Client::_fillHeaders(const std::vector<std::string> &header_lines, std::unordered_map<std::string, std::string> &filled_headers)
{
	for (const std::string &line : header_lines)
	{
		// Assign key to everything before the ':' seperator
		size_t i = line.find(":");
		if (i == std::string::npos || i == 0) return false;
		std::string key = line.substr(0, i);

		// Skip ':'
		i++;

		// Capitalize letters and replace '-' with '_'
		for (size_t j = 0; j < key.size(); j++)
		{
			key[j] = toupper(key[j]);

			if (key[j] == '-')
				key[j] = '_';
		}

		std::string value = line.substr(i);
		Utils::trim(value, " \t");

		// Check if key is a duplicate
		if (filled_headers.contains(key)) return false;

		// Add key and value to the map
		filled_headers.emplace(key, value);
		// L::info(std::string("    Header key: '") + key + "', value: '" + value + "'");
	}

	return true;
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

		if (!Utils::parseNumber(content_length_it->second, this->content_length, std::numeric_limits<size_t>::max())) throw ClientException(Status::BAD_REQUEST);

		L::info(std::string("    Content length: ") + std::to_string(this->content_length));
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
			// TODO: Uppercase
			this->server_name = host;
		}
		else
		{
			// If there is no server name before the colon
			if (colon_index == 0) throw ClientException(Status::BAD_REQUEST);

			// TODO: Uppercase
			this->server_name = host.substr(0, colon_index);

			// nginx doesn't care what comes after the colon, if anything
		}
	}

	const auto &content_type_it = this->headers.find("CONTENT_TYPE");
	if (content_type_it != this->headers.end())
	{
		this->content_type = content_type_it->second;
	}
}

void Client::_parseBodyAppend(const std::string &extra_body)
{
	if (this->_is_chunked)
	{
		L::info(std::string("Parsing chunked body substring"));
		this->_chunked_body_buffer.append(extra_body);
		while (true)
		{
			if (this->_chunked_read_state == ChunkedReadState::READING_CONTENT_LEN)
			{
				// Check if current buffer has anything else than a hexadecimal character
				if (this->_chunked_body_buffer.find_first_not_of("1234567890abcdefABCDEF") == std::string::npos)
					break;

				_hexToNum(this->_chunked_body_buffer, this->_chunked_remaining_content_length);

				this->content_length += this->_chunked_remaining_content_length; // TODO: This isn't needed but it's nice to just update it as appropriate (Maybe want to delete because it isn't needed)
				this->_chunked_read_state = ChunkedReadState::READING_CONTENT_LEN_ENDLINE;
			}
			if (this->_chunked_read_state == ChunkedReadState::READING_BODY)
			{
				// If not all of extra_body should fit into the body
				if (this->_chunked_body_buffer.size() >= this->_chunked_remaining_content_length)
				{
					this->body.append(this->_chunked_body_buffer, 0, this->_chunked_remaining_content_length);
					this->_chunked_body_buffer.erase(0, this->_chunked_remaining_content_length);
					this->_chunked_remaining_content_length = 0;
					this->_chunked_read_state = ChunkedReadState::READING_BODY_ENDLINE;
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
			if (this->_chunked_read_state == ChunkedReadState::READING_CONTENT_LEN_ENDLINE || this->_chunked_read_state == ChunkedReadState::READING_BODY_ENDLINE)
			{
				if (this->_chunked_body_buffer.size() < 2)
					break;

				// If buffer starts with "\r\n" (as expected)
				if (this->_chunked_body_buffer[0] == '\r' && this->_chunked_body_buffer[1] == '\n')
				{
					this->_chunked_body_buffer.erase(0, 2);

					// If at the end of chunked requests
					if (this->_chunked_remaining_content_length == 0 && this->_chunked_read_state == ChunkedReadState::READING_CONTENT_LEN_ENDLINE)
					{
						this->client_to_server_state = ClientToServerState::DONE;
					}
					else if (this->_chunked_read_state == ChunkedReadState::READING_CONTENT_LEN_ENDLINE)
					{
						this->_chunked_read_state = ChunkedReadState::READING_BODY;
					}
					else if (this->_chunked_read_state == ChunkedReadState::READING_BODY_ENDLINE)
					{
						this->_chunked_read_state = ChunkedReadState::READING_CONTENT_LEN;
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
		L::info(std::string("    Parsing non-chunked body substring"));
		// If not all of extra_body should fit into the body
		if (this->body.size() + extra_body.size() >= this->content_length)
		{
			size_t needed = this->content_length - this->body.size();
			this->body.append(extra_body.begin(), extra_body.begin() + needed);
			this->client_to_server_state = ClientToServerState::DONE;
		}
		// If all of extra_body should fit into the body
		else
			this->body.append(extra_body);
	}

	if (this->body.size() > this->client_max_body_size)
	{
		throw ClientException(Status::REQUEST_ENTITY_TOO_LARGE);
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

std::vector<std::string> Client::_getSortedEntryNames(const std::string &path, bool only_return_directory_entries)
{
	std::vector<std::string> names;

	DIR *dirp = opendir(path.c_str());
	if (dirp == NULL) throw ClientException(Status::NOT_FOUND);

	const dirent *dp;
	while ((dp = readdir(dirp)) != NULL)
	{
		std::string name = dp->d_name;

		if (name == ".")
		{
			continue;
		}

		if (only_return_directory_entries)
		{
			if (dp->d_type != DT_DIR)
			{
				continue;
			}
			name += "/";
		}
		else
		{
			if (dp->d_type == DT_DIR)
			{
				continue;
			}
		}

		names.push_back(name);
	}

	closedir(dirp);

	std::sort(names.begin(), names.end());

	return names;
}

void Client::_addResponseHeader(const std::string &response_header_key, const std::string &response_header_value)
{
	this->response += response_header_key + ": " + response_header_value + "\r\n";
}

std::string Client::_getFileExtension(const std::string &path)
{
	size_t period_index = path.find_last_of(".");
	if (period_index == path.npos) return "";
	return path.substr(period_index);
}

std::string Client::_getFileName(const std::string &path)
{
	size_t slash_index = path.find_last_of("/");
	if (slash_index == path.npos || slash_index + 1 >= path.length()) return "";
	return path.substr(slash_index + 1);
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
