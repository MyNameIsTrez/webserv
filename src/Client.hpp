#pragma once

#include "Status.hpp"

#include <cstddef>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

class Client
{
  public:
    enum class ClientToServerState
    {
        HEADER,
        BODY,
        DONE
    };

    enum class ServerToCGIState
    {
        NOT_WRITING,
        WRITING,
        DONE
    };

    enum class CGIToServerState
    {
        NOT_READING,
        READING,
        DONE
    };

    enum class ServerToClientState
    {
        NOT_WRITING,
        WRITING,
        DONE
    };

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

    void extractCGIDocumentResponseHeaders(void);

    std::string getRequestMethodString(void) const;

    int server_fd;

    Status::Status status;
    static const char *reason_phrase_table[];

    struct ClientException : public std::runtime_error
    {
        ClientException(Status::Status status_)
            : runtime_error("Client exception: " + std::to_string(status_) + " " + reason_phrase_table[status_]),
              status(status_)
        {
        }

        Status::Status status;
    };

    ClientToServerState client_to_server_state;
    ServerToCGIState server_to_cgi_state;
    CGIToServerState cgi_to_server_state;
    ServerToClientState server_to_client_state;

    enum class RequestMethod
    {
        NONE,
        GET,
        POST,
        DELETE
    };

    RequestMethod request_method;
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
    bool cgi_killed;
    bool being_removed;
    std::string redirect;
    std::string server_name;
    std::string unuppercased_server_name;
    std::string server_port;
    std::string content_type;
    size_t content_length;

  private:
    Client(void);

    std::vector<std::string> _getHeaderLines(void);
    void _parseRequestLine(const std::string &line);

    RequestMethod _getRequestMethodEnumFromString(const std::string &request_method_string) const;

    void _decodeRequestTarget(void);

    bool _isValidProtocol(void);

    bool _fillHeaders(const std::vector<std::string> &header_lines,
                      std::unordered_map<std::string, std::string> &filled_headers);
    void _useHeaders(void);

    void _parseBodyAppend(const std::string &extra_body);
    void _hexToNum(std::string &line, size_t &num);

    std::vector<std::string> _getSortedEntryNames(const std::string &path, bool only_return_directory_entries);

    void _addResponseHeader(const std::string &response_header_key, const std::string &response_header_value);

    std::string _getFileExtension(const std::string &path);
    std::string _getFileName(const std::string &path);

    std::string _custom_reason_phrase;

    std::string _response_content_type;
    std::string _header;

    bool _is_chunked;
    size_t _chunked_remaining_content_length;
    std::string _chunked_body_buffer;

    enum class ChunkedReadState
    {
        READING_CONTENT_LEN,
        READING_BODY,
        READING_CONTENT_LEN_ENDLINE,
        READING_BODY_ENDLINE
    } _chunked_read_state;
};
