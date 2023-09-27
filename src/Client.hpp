#pragma once

#include <cstddef>
#include <poll.h>
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
namespace ClientReadState
{
	enum ClientReadState
	{
		HEADER,
		BODY,
		DONE
	};
}

namespace CGIWriteState
{
	enum CGIWriteState
	{
		NOT_WRITING,
		WRITING_TO_CGI,
		DONE
	};
}

namespace CGIReadState
{
	enum CGIReadState
	{
		NOT_READING,
		READING_FROM_CGI,
		DONE
	};
}

namespace ClientWriteState
{
	enum ClientWriteState
	{
		NOT_WRITING,
		WRITING_TO_CLIENT,
		DONE
	};
}

class Client
{
public:
	Client(int client_fd);
	Client(Client const &src);
	virtual ~Client(void);
	Client &operator=(Client const &src);

	bool appendReadString(char *received, ssize_t bytes_read);

	void prependResponseHeader();

	ClientReadState::ClientReadState client_read_state;
	CGIWriteState::CGIWriteState cgi_write_state;
	CGIReadState::CGIReadState cgi_read_state;
	ClientWriteState::ClientWriteState client_write_state;

	std::string request_method;
	std::string path;
	std::string protocol;
	std::unordered_map<std::string, std::string> header_map;
	std::string body;
	size_t body_index;
	std::string response;
	size_t response_index;
	int client_fd;
	int server_to_cgi_fd;
	int cgi_to_server_fd;
	pid_t cgi_pid;
	int cgi_exit_status;

private:
	Client(void);

	bool _parseHeaders(void);
	bool _parseStartLine(std::string line);
	bool _parseBodyAppend(const std::string &received);
	void _generateEnv();
	// std::string _replace_all(std::string input, const std::string& needle, const std::string& replacement);

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
};
