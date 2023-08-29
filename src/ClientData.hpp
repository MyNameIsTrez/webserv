#pragma once

#include <string>

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

class ClientData
{
public:
	ClientData(void);
	ClientData(int client_fd);
	ClientData(ClientData const &src);
	virtual ~ClientData(void);
	ClientData &operator=(ClientData const &src);

	void readSocket(void);

	enum State
	{
		HEADER = 0,
		BODY,
		DONE
	} state;
	enum RequestMethod
	{
		UNKOWN = 0,
		GET,
		POST,
		DELETE
	} request_method;
	std::string uri;
	std::string body;
	size_t respond_index;

private:
	std::string _header;
	size_t _content_length; // TODO: See if needed
	int _fd;
};
