#pragma once

#include "Status.hpp"

#include <cstddef>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// TODO: Remove this comment block when handing in
/*
// whole socket input
Header string
body string
client_fd
enum for request method
content length
size_t for sent characters (i)
// Could maybe add enum for reading body vs header (not sure if has to) // State
// Ignore chunked request doody for now (will be my part probably)
*/

// TODO: Use class enums?
namespace ClientToServerState
{
	enum ClientToServerState
	{
		HEADER,
		BODY,
		DONE
	};
}

namespace ServerToCGIState
{
	enum ServerToCGIState
	{
		NOT_WRITING,
		WRITING,
		DONE
	};
}

namespace CGIToServerState
{
	enum CGIToServerState
	{
		NOT_READING,
		READING,
		DONE
	};
}

namespace ServerToClientState
{
	enum ServerToClientState
	{
		NOT_WRITING,
		WRITING,
		DONE
	};
}

class Client
{
public:
	Client(int client_fd, int server_fd, const std::string &server_port, const size_t &client_max_body_size);
	Client(Client const &src);
	virtual ~Client(void);
	Client &operator=(Client const &src);

	void appendReadString(char *received, ssize_t bytes_read);

	void respondWithError(const std::unordered_map<Status::Status, std::string> &error_pages);
	void respondWithFile(const std::string &path);
	void respondWithDirectoryListing(const std::string &path);
	void respondWithCreateFile(const std::string &path);
	void respondWithDeleteFile(const std::string &path);

	void prependResponseHeader(void);

	int server_fd;

	Status::Status status;

	static const char *status_text_table[];

	struct ClientException : public std::runtime_error
	{
		ClientException(Status::Status status_)
			: runtime_error("Client exception: " + std::to_string(status_) + " " + status_text_table[status_])
			, status(status_)
		{
		}

		Status::Status status;
	};

	ClientToServerState::ClientToServerState client_read_state;
	ServerToCGIState::ServerToCGIState cgi_write_state;
	CGIToServerState::CGIToServerState cgi_read_state;
	ServerToClientState::ServerToClientState client_write_state;

	std::string request_method;
	std::string request_target;
	std::string protocol;
	std::unordered_map<std::string, std::string> headers;
	std::string body;
	size_t body_index;
	size_t client_max_body_size;
	std::string response;
	size_t response_index;
	int client_fd;
	int server_to_cgi_fd;
	int cgi_to_server_fd;
	pid_t cgi_pid;
	int cgi_exit_status;
	std::string redirect;
	std::string server_name;

private:
	Client(void);

	std::vector<std::string> _getHeaderLines(void);
	void _parseRequestLine(const std::string &line);

	bool _isValidRequestMethod(void);
	bool _isValidRequestTarget(void);
	bool _isValidProtocol(void);

	void _fillHeaders(const std::vector<std::string> &header_lines);
	void _useHeaders(void);

	void _parseBodyAppend(const std::string &extra_body);
	void _hexToNum(std::string &line, size_t &num);

	std::vector<std::string> _getSortedEntryNames(const std::string &path, bool only_return_directory_entries);

	void _addResponseHeader(const std::string &response_header_key, const std::string &response_header_value);

	void _addStatusLine(void);

	std::string _getFileExtension(const std::string &path);
	std::string _getFileName(const std::string &path);

	// std::string _replaceAll(std::string input, const std::string& needle, const std::string& replacement);

	std::string _response_content_type;
	size_t _content_length;
	std::string _header;

	bool _is_chunked;
	size_t _chunked_remaining_content_length;
	std::string _chunked_body_buffer;
	// TODO: Put in namespace?
	enum ChunkedReadState
	{
		READING_CONTENT_LEN,
		READING_BODY,
		READING_CONTENT_LEN_ENDLINE,
		READING_BODY_ENDLINE
	} _chunked_read_state;

	std::string _server_port;
	std::string _response_headers;
};
