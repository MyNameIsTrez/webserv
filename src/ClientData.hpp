#pragma once

#include <string>
#include <unordered_map>

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

namespace ReadState
{
	enum ReadState
	{
		HEADER,
		BODY,
		READING_FROM_CGI,
		DONE
	};
}

namespace WriteState
{
	enum WriteState
	{
		NOT_WRITING,
		WRITING_TO_CGI,
		WRITING_TO_CLIENT,
		DONE
	};
}

class ClientData
{
public:
	ClientData(void);
	ClientData(ClientData const &src);
	virtual ~ClientData(void);
	ClientData &operator=(ClientData const &src);

	ClientData(int client_fd);

	bool readSocket(void);

	// enum ReadState
	// {
	// 	HEADER,
	// 	BODY,
	// 	READING_FROM_CGI,
	// 	READING_DONE
	// } read_state;

	// enum WriteState
	// {
	// 	NOT_WRITING,
	// 	WRITING_TO_CGI,
	// 	WRITING_TO_CLIENT,
	// 	WRITING_DONE
	// } write_state;

	ReadState::ReadState read_state;
	WriteState::WriteState write_state;

	std::string request_method;
	std::string path;
	std::string protocol;
	std::unordered_map<std::string, std::string> header_map;
	std::string body;
	size_t response_index;

private:
	bool parseHeaders(void);
	bool parseStartLine(std::string line);

	std::string _header;
	size_t _content_length; // TODO: See if needed
	int _fd;
};
