#include "ClientData.hpp"
#include <unistd.h>

#define MAX_RECEIVED_LEN 5

/*	Orthodox Canonical Form */

ClientData::ClientData(void)
	: state(HEADER),
	  request_method(UNKOWN),
	  respond_index(0),
	  _content_length(0),
	  _fd(0)
{
}

ClientData::ClientData(ClientData const &src)
	: state(src.state),
	  request_method(src.request_method),
	  uri(src.uri),
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
	this->state = src.state;
	this->request_method = src.request_method;
	this->uri = src.uri;
	this->body = src.body;
	this->respond_index = src.respond_index;
	this->_header = src._header;
	this->_content_length = src._content_length;
	this->_fd = src._fd;
	return (*this);
}

/*	Other constructors */

ClientData::ClientData(int client_fd) : state(HEADER), request_method(UNKOWN), respond_index(0), _content_length(0), _fd(client_fd)
{
}

/*	Member functions */

void ClientData::readSocket(void)
{
	char received[MAX_RECEIVED_LEN + 1];
	bzero(received, MAX_RECEIVED_LEN + 1);

	ssize_t bytes_read = read(this->_fd, received, MAX_RECEIVED_LEN);
	if (bytes_read == -1)
	{
		perror("read");
		exit(EXIT_FAILURE);
	}
	// TODO:
	// if (bytes_read == 0)
	// {
	// }

	if (this->state == HEADER)
	{
		// TODO: Handle MAX_RECEIVED_LEN < 4
		// TODO: MAYBE BETTER FORMATTING BT PROBABLY KEEP THE LOGIC
		// TODO: Handle to copy to _header
		if (this->_header[this->_header.size() - 3] == '\r' && this->_header[this->_header.size() - 2] == '\n' && this->_header[this->_header.size() - 1] == '\r' && received[0] == '\n')
		{
			// _header += received_partial;

			// if (...)
			// {
			// 	this->state = BODY;
			// }
		}
		if (this->_header[this->_header.size() - 2] == '\r' && this->_header[this->_header.size() - 1] == '\n' && received[0] == '\r' && received[1] == '\n')
		{
			// _header += received_partial;

			// if (...)
			// {
			// 	this->state = BODY;
			// }
		}
		if (this->_header[this->_header.size() - 1] == '\r' && received[0] == '\n' && received[1] == '\r' && received[2] == '\n')
		{
			// _header += received_partial;

			// if (...)
			// {
			// 	this->state = BODY;
			// }
		}
		if (received[0] == '\r' && received[1] == '\n' && received[2] == '\r' && received[3] == '\n')
		{
			// _header.append(received, 0, rnrn_index + 2) // TODO: use this function
			// _header += received_partial;

			// if (...)
			// {
			// 	this->state = BODY;
			//	body.append(received, rnrn_index + 4, received.npos);
			// }
		}
	}
	else
	{
		body += received;
	}

	// printf("\nRead %zd bytes:\n-------------------------\n%s\n-------------------------\n",
	// 	   bytes_read, received);
}
