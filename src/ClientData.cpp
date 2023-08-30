#include "ClientData.hpp"
#include <unistd.h>
#include <vector>
#include <assert.h> // TODO: DELETE
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
	size_t path_end_pos = line.find_last_of(" ", request_method_end_pos, line.size() - request_method_end_pos);
	if (path_end_pos == std::string::npos)
		return (false);
	this->path = line.substr(request_method_end_pos, path_end_pos - request_method_end_pos);
	path_end_pos++;

	// Set and validate the protocol
	// TODO: Check if format of protocol is correct
	this->protocol = line.substr(path_end_pos);
	return (true);
}

void ClientData::parseHeaders(void)
{
	std::vector<std::string> split;
	size_t pos;
	size_t start = 0;

	while ((pos = this->_header.find("\r\n", start)) != std::string::npos)
	{
		split.push_back(this->_header.substr(start, pos - start));
		start = pos + 2;
	}

	this->parseStartLine(split[0]); // TODO: Use return value

	// TODO: Make parser for headers to unordered_map
}

void ClientData::readSocket(void)
{
	char received[MAX_RECEIVED_LEN];   // NOTE: Deleted + 1
	bzero(received, MAX_RECEIVED_LEN); // NOTE: Deleted + 1

	ssize_t bytes_read = read(this->_fd, received, MAX_RECEIVED_LEN);
	if (bytes_read == -1)
	{
		perror("read");
		exit(EXIT_FAILURE);
	}
	// TODO: Handle this
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
			parseHeaders();
		}
		// "\r\n" + "\r\n"
		else if (this->_header.size() >= 2 && bytes_read >= 2 && this->_header[this->_header.size() - 2] == '\r' && this->_header[this->_header.size() - 1] == '\n' && received[0] == '\r' && received[1] == '\n')
		{
			this->_header.append(received, 0, 2);
			this->body.append(received, 2, bytes_read - 2);
			this->read_state = BODY;
			parseHeaders();
		}
		// "\r" + "\n\r\n"
		else if (this->_header.size() >= 1 && bytes_read >= 3 && this->_header[this->_header.size() - 1] == '\r' && received[0] == '\n' && received[1] == '\r' && received[2] == '\n')
		{
			this->_header.append(received, 0, 3);
			this->body.append(received, 3, bytes_read - 3);
			this->read_state = BODY;
			parseHeaders();
		}
		else
		{
			char const rnrn[] = "\r\n\r\n";
			char *ptr = std::search(received, received + bytes_read, rnrn, rnrn + sizeof(rnrn) - 1);

			// If no \r\n\r\n found
			if (ptr == received + bytes_read)
			{
				this->_header += received;
			}
			// If \r\n\r\n found
			else
			{
				this->_header.append(received, ptr + 2);					   // Include "\r\n"
				this->body.append(ptr + 4, bytes_read - (ptr + 4 - received)); // Skip "\r\n\r\n"
				this->read_state = BODY;
				parseHeaders();
			}
		}
	}
	else if (this->read_state == BODY)
	{
		body += received;
	}
	else
	{
		// Should be unreachable
		assert(false); // TODO: REMOVE
	}

	// printf("\nRead %zd bytes:\n-------------------------\n%s\n-------------------------\n",
	// 	   bytes_read, received);
}
