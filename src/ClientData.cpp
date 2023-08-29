#include "ClientData.hpp"
#include <unistd.h>

#define MAX_RECEIVED_LEN 300

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

	// printf("\nRead %zd bytes:\n-------------------------\n%s\n-------------------------\n",
	// 	   bytes_read, received);
}
