#include "Server.hpp"

#include "Client.hpp"
#include "Logger.hpp"
#include "Throwing.hpp"
#include "Utils.hpp"
#include "config/Config.hpp"
#include "defines.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/stat.h>
#include <unordered_set>

namespace L = Logger;
namespace T = Throwing;

bool shutting_down_gracefully = false;

int Server::_sig_chld_pipe[2];

Server::Server(const Config &config)
    : _config(config), _bind_fd_to_server_indices(), _bind_fd_to_port(), _cgi_pid_to_client_fd(), _fd_to_client_index(),
      _fd_to_pfd_index(), _fd_to_fd_type(), _clients(), _pfds()
{
    for (const auto &it : _config.bind_info_to_server_indices)
    {
        const auto &bind_info = it.first;
        const auto &server_indices = it.second;

        int bind_fd = T::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        _bind_fd_to_server_indices.emplace(bind_fd, server_indices);

        L::info(std::string("Adding address ") + std::to_string(bind_info.s_addr) + " with port " +
                std::to_string(ntohs(bind_info.sin_port)));
        _bind_fd_to_port.emplace(bind_fd, std::to_string(ntohs(bind_info.sin_port)));

        // Prevents "bind: Address already in use" error
        int option = 1; // "the parameter should be non-zero to enable a boolean option"
        T::setsockopt(bind_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

        sockaddr_in sockaddrin_entry{};
        sockaddrin_entry.sin_family = AF_INET;
        sockaddrin_entry.sin_addr.s_addr = bind_info.s_addr;
        sockaddrin_entry.sin_port = bind_info.sin_port;
        sockaddr *sockaddr_entry = reinterpret_cast<sockaddr *>(&sockaddrin_entry);

        T::bind(bind_fd, sockaddr_entry, sizeof(sockaddrin_entry));

        T::listen(bind_fd, _config.connection_queue_length);

        _addFd(bind_fd, FdType::SERVER, POLLIN);

        L::info(std::string("Added bind fd ") + std::to_string(bind_fd));
    }

    T::signal(SIGINT, _sigIntHandler);
    T::signal(SIGPIPE, SIG_IGN);

    T::pipe(_sig_chld_pipe);
    _addFd(_sig_chld_pipe[PIPE_READ_INDEX], FdType::SIG_CHLD, POLLIN);
    L::info(std::string("Added _sig_chld_pipe[PIPE_READ_INDEX] fd ") + std::to_string(_sig_chld_pipe[PIPE_READ_INDEX]));

    T::signal(SIGCHLD, _sigChldHandler);
}

Server::~Server(void)
{
}

void Server::run(void)
{
    bool servers_active = true;

    std::unordered_set<int> seen_fds;

    while (true)
    {
        if (shutting_down_gracefully && servers_active)
        {
            L::info("\nShutting down gracefully...");
            servers_active = false;
            _shutDownGracefully();
        }

        // If the only fd left in _pfds is _sig_chld_pipe[PIPE_READ_INDEX], return
        if (_pfds.size() == 1)
        {
            _removeFd(_sig_chld_pipe[PIPE_READ_INDEX]);
            L::info(std::string("Gootbye"));
            return;
        }
        else if (shutting_down_gracefully)
        {
            for (pollfd pfd : _pfds)
            {
                L::info(std::string("  Waiting for poll fd ") + std::to_string(pfd.fd));
            }
        }

        L::info(std::string("Waiting for an event..."));

        int event_count = poll(_pfds.data(), _pfds.size(), -1);
        if (event_count == -1)
        {
            if (errno == EINTR)
            {
                L::info(std::string("  poll() got interrupted by a signal handler"));
                continue;
            }
            throw T::SystemException("poll");
        }
        else if (event_count == 0)
        {
            L::info("  poll() timed out");
        }

        _printContainerSizes();

        seen_fds.clear();

        for (nfds_t pfd_index = _pfds.size(); pfd_index > 0;)
        {
            pfd_index--;
            _processPfd(pfd_index, seen_fds);
        }
    }
}

// vector=[ A, B, C, D ] with index=1 results in [ A, D, C ]
template <typename T> void Server::_swapRemove(T &vector, size_t index)
{
    vector[index] = vector.back();
    vector.pop_back();
}

void Server::_shutDownGracefully(void)
{
    for (auto [fd, fd_type] : _fd_to_fd_type)
    {
        if (fd_type == FdType::SERVER)
        {
            size_t pfd_index = _fd_to_pfd_index.at(fd);
            _fd_to_pfd_index[_pfds.back().fd] = pfd_index;
            _swapRemove(_pfds, pfd_index);

            _fd_to_pfd_index.erase(fd);
            // We're not erasing from _fd_to_fd_type, since we're looping through it

            T::close(fd);
        }
    }
}

void Server::_printContainerSizes(void)
{
    L::info(std::string("MAPS: ") + "_bind_fd_to_server_indices=" + std::to_string(_bind_fd_to_server_indices.size()) +
            ", _bind_fd_to_port=" + std::to_string(_bind_fd_to_port.size()) +
            ", _cgi_pid_to_client_fd=" + std::to_string(_cgi_pid_to_client_fd.size()) +
            ", _fd_to_client_index=" + std::to_string(_fd_to_client_index.size()) + ", _fd_to_pfd_index=" +
            std::to_string(_fd_to_pfd_index.size()) + ", _fd_to_fd_type=" + std::to_string(_fd_to_fd_type.size()) +
            " | VECTORS: " + "_clients=" + std::to_string(_clients.size()) + ", _pfds=" + std::to_string(_pfds.size()));
}

void Server::_processPfd(size_t pfd_index, std::unordered_set<int> &seen_fds)
{
    // If this pfd didn't have any event
    if (_pfds[pfd_index].revents == 0)
    {
        return;
    }

    int fd = _pfds[pfd_index].fd;

    // If this pfd got removed
    if (!_fd_to_pfd_index.contains(fd))
    {
        return;
    }

    // If we've already iterated over this fd in this _pfds loop
    // This never happened in practice, but it doesn't hurt ¯\_(ツ)_/¯
    if (seen_fds.contains(fd))
    {
        return;
    }
    seen_fds.emplace(fd);

    FdType fd_type = _fd_to_fd_type.at(fd);

    _printEvents(_pfds[pfd_index], fd_type);

    if (_pfds[pfd_index].revents & POLLNVAL)
    {
        assert(false);
    }

    // If we are trying to write to a CGI script that closed its stdin,
    // or we are trying to write to a disconnected client
    if (_pfds[pfd_index].revents & POLLERR)
    {
        _handlePollerr(fd, fd_type);
        return;
    }

    // If the CGI script closed its stdout
    if (_pfds[pfd_index].revents & POLLHUP)
    {
        assert(fd_type == FdType::CGI_TO_SERVER);

        // If the server has read everything from the CGI script
        if (!(_pfds[pfd_index].revents & POLLIN))
        {
            _pollhupCGIToServer(fd);
            return;
        }
    }

    // If we can read
    if (_pfds[pfd_index].revents & POLLIN)
    {
        bool skip_client = false;
        _handlePollin(fd, fd_type, skip_client);
        if (skip_client)
            return;
    }

    // If we can write
    if (_pfds[pfd_index].revents & POLLOUT)
    {
        _handlePollout(fd, fd_type);
    }
}

void Server::_printEvents(const pollfd &pfd, FdType fd_type)
{
    L::info(std::string("  fd: " + std::to_string(pfd.fd))); // TODO: Remove
    L::info(std::string("  fd: " + std::to_string(pfd.fd)) + ", fd_type: " + std::to_string(int(fd_type)) +
            ", client_index: " +
            std::to_string(
                (fd_type == FdType::SERVER || fd_type == FdType::SIG_CHLD) ? -1 : _fd_to_client_index.at(pfd.fd)) +
            ", client_fd: " +
            std::to_string((fd_type == FdType::SERVER || fd_type == FdType::SIG_CHLD)
                               ? -1
                               : _clients.at(_fd_to_client_index.at(pfd.fd)).client_fd) +
            ", revents:" + ((pfd.revents & POLLIN) ? " POLLIN" : "") + ((pfd.revents & POLLOUT) ? " POLLOUT" : "") +
            ((pfd.revents & POLLHUP) ? " POLLHUP" : "") + ((pfd.revents & POLLNVAL) ? " POLLNVAL" : "") +
            ((pfd.revents & POLLPRI) ? " POLLPRI" : "") + ((pfd.revents & POLLRDBAND) ? " POLLRDBAND" : "") +
            ((pfd.revents & POLLRDNORM) ? " POLLRDNORM" : "") + ((pfd.revents & POLLWRBAND) ? " POLLWRBAND" : "") +
            ((pfd.revents & POLLWRNORM) ? " POLLWRNORM" : "") + ((pfd.revents & POLLERR) ? " POLLERR" : ""));
}

// This is called in many spots due to the fact
// that a client pointer can dangle whenever the _clients vector resizes
Client &Server::_getClient(int fd)
{
    size_t client_index = _fd_to_client_index.at(fd);
    return _clients[client_index];
}

void Server::_removeClientFd(int &fd)
{
    assert(fd != -1);

    _fd_to_client_index.erase(fd);

    _removeFd(fd);
}

void Server::_removeFd(int &fd)
{
    assert(fd != -1);

    L::info(std::string("    Removing fd ") + std::to_string(fd));

    size_t pfd_index = _fd_to_pfd_index.at(fd);
    _fd_to_pfd_index[_pfds.back().fd] = pfd_index;
    _swapRemove(_pfds, pfd_index);

    _fd_to_pfd_index.erase(fd);
    _fd_to_fd_type.erase(fd);

    T::close(fd);

    fd = -1;
}

void Server::_removeServerToCGIFd(int fd)
{
    assert(fd != -1);

    Client &client = _getClient(fd);

    if (client.server_to_cgi_fd == -1)
    {
        return;
    }

    // TODO: Remove this since we're calling _removeClientFd() anyways?
    // L::info(std::string("    Disabling server_to_cgi POLLOUT"));
    // size_t pfd_index = _fd_to_pfd_index.at(client.server_to_cgi_fd);
    // _disableEvent(pfd_index, POLLOUT);

    L::info(std::string("  Removing server_to_cgi_fd ") + std::to_string(client.server_to_cgi_fd));

    _removeClientFd(client.server_to_cgi_fd);

    client.server_to_cgi_state = Client::ServerToCGIState::DONE;
}

void Server::_removeCGIToServerFd(int fd)
{
    assert(fd != -1);

    Client &client = _getClient(fd);

    if (client.cgi_to_server_fd == -1)
    {
        return;
    }

    L::info(std::string("  Removing cgi_to_server_fd ") + std::to_string(client.cgi_to_server_fd));

    _removeClientFd(client.cgi_to_server_fd);

    client.cgi_to_server_state = Client::CGIToServerState::DONE;
}

void Server::_enableEvent(size_t pfd_index, short int event)
{
    L::info(std::string("    Enabling event ") + std::to_string(event) + " on pfd_index " + std::to_string(pfd_index));
    _pfds[pfd_index].events |= event;
}

void Server::_disableEvent(size_t pfd_index, short int event)
{
    L::info(std::string("    Disabling event ") + std::to_string(event) + " on pfd_index " + std::to_string(pfd_index));
    _pfds[pfd_index].events &= ~event;
    _pfds[pfd_index].revents &= ~event;
}

void Server::_enableWritingToClient(Client &client)
{
    L::info(std::string("    In _enableWritingToClient()"));
    size_t client_pfd_index = _fd_to_pfd_index.at(client.client_fd);
    _enableEvent(client_pfd_index, POLLOUT);

    client.server_to_client_state = Client::ServerToClientState::WRITING;
}

void Server::_enableWritingToCGI(Client &client)
{
    assert(client.server_to_cgi_state != Client::ServerToCGIState::DONE);
    assert(client.server_to_cgi_fd != -1);

    L::info(std::string("    In _enableWritingToCGI()"));
    size_t server_to_cgi_pfd_index = _fd_to_pfd_index.at(client.server_to_cgi_fd);
    _enableEvent(server_to_cgi_pfd_index, POLLOUT);

    client.server_to_cgi_state = Client::ServerToCGIState::WRITING;
}

void Server::_disableReadingFromClient(Client &client)
{
    L::info(std::string("    In _disableReadingFromClient()"));
    size_t client_pfd_index = _fd_to_pfd_index.at(client.client_fd);
    _disableEvent(client_pfd_index, POLLIN);

    client.client_to_server_state = Client::ClientToServerState::DONE;
}

void Server::_addClientFd(int fd, size_t client_index, FdType fd_type, short int events)
{
    assert(fd != -1);

    _fd_to_client_index.emplace(fd, client_index);

    _addFd(fd, fd_type, events);
}

void Server::_addFd(int fd, FdType fd_type, short int events)
{
    assert(fd != -1);

    L::info(std::string("    Adding fd ") + std::to_string(fd));

    _fd_to_fd_type.emplace(fd, fd_type);
    _fd_to_pfd_index.emplace(fd, _pfds.size());

    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = events;
    _pfds.push_back(pfd);
}

void Server::_handlePollerr(int fd, FdType fd_type)
{
    if (fd_type == FdType::SERVER_TO_CGI)
    {
        _removeServerToCGIFd(fd);
    }
    else if (fd_type == FdType::CLIENT)
    {
        _removeClient(fd);
    }
    else
    {
        assert(false);
    }
}

void Server::_pollhupCGIToServer(int fd)
{
    L::info(std::string("  In _pollhupCGIToServer()"));

    Client &client = _getClient(fd);

    client.client_to_server_state = Client::ClientToServerState::DONE;

    _removeServerToCGIFd(fd);
    _removeCGIToServerFd(fd);

    if (client.cgi_pid == -1)
    {
        _cgiEnd(client);
    }
}

void Server::_cgiEnd(Client &client)
{
    L::debug("client.cgi_exit_status: " + std::to_string(client.cgi_exit_status));
    if (client.cgi_exit_status != 0)
    {
        _respondClientException(Client::ClientException(Status::INTERNAL_SERVER_ERROR), client);
    }
    else
    {
        client.extractCGIDocumentResponseHeaders();
        client.prependResponseHeader();
        _enableWritingToClient(client);
    }
}

void Server::_handlePollin(int fd, FdType fd_type, bool &skip_client)
{
    if (fd_type == FdType::SERVER)
    {
        _acceptClient(fd);
    }
    else
    {
        if (fd_type == FdType::SIG_CHLD)
        {
            _reapChild();

            // TODO: Do we need to manually call methods here that remove server_to_cgi and cgi_to_server?

            skip_client = true;
        }
        else
        {
            assert(fd_type == FdType::CLIENT || fd_type == FdType::CGI_TO_SERVER);

            Client &client = _getClient(fd);

            try
            {
                _readFd(client, fd, fd_type, skip_client);
            }
            catch (const Client::ClientException &e)
            {
                _respondClientException(e, client);

                skip_client = true;
            }
        }
    }
}

void Server::_acceptClient(int server_fd)
{
    int client_fd = T::accept(server_fd, NULL, NULL);
    L::info(std::string("    Accepted client fd ") + std::to_string(client_fd));

    _addClientFd(client_fd, _clients.size(), FdType::CLIENT, POLLIN);

    const std::string &server_port = _bind_fd_to_port.at(server_fd);

    _clients.push_back(Client(client_fd, server_fd, server_port, _config.client_max_body_size));

    std::cout << "Added a client; " << _clients.size() << " clients now connected" << std::endl;
}

void Server::_reapChild(void)
{
    L::info(std::string("    In _reapChild()"));

    char dummy;
    T::read(_sig_chld_pipe[PIPE_READ_INDEX], &dummy, 1);

    // Reaps all children that have exited
    // waitpid() returns 0 if no more children can be reaped right now
    // WNOHANG guarantees that this call doesn't block
    // This is done in a loop, since signals aren't queued: https://stackoverflow.com/a/45809843/13279557
    // TODO: Are there other options we should use?
    int child_exit_status;
    pid_t child_pid = T::waitpid(-1, &child_exit_status, WNOHANG);

    // Reached when the client disconnects before the CGI has finished
    if (!_cgi_pid_to_client_fd.contains(child_pid))
    {
        return;
    }

    int client_fd = _cgi_pid_to_client_fd.at(child_pid);
    size_t client_index = _fd_to_client_index.at(client_fd);
    Client &client = _clients.at(client_index);

    _cgi_pid_to_client_fd.erase(client.cgi_pid);
    client.cgi_pid = -1;

    assert(client.client_to_server_state == Client::ClientToServerState::DONE);
    assert(client.server_to_cgi_state == Client::ServerToCGIState::DONE);
    assert(client.cgi_to_server_state != Client::CGIToServerState::NOT_READING);
    assert(client.server_to_client_state == Client::ServerToClientState::NOT_WRITING);

    if (WIFEXITED(child_exit_status))
    {
        client.cgi_exit_status = WEXITSTATUS(child_exit_status);
    }

    if (client.being_removed)
    {
        _removeClient(client_fd);
    }
    else if (client.cgi_to_server_state == Client::CGIToServerState::DONE)
    {
        _cgiEnd(client);
    }
}

void Server::_readFd(Client &client, int fd, FdType fd_type, bool &skip_client)
{
    char received[MAX_RECEIVED_LEN] = {};

    L::info(std::string("    About to call read(") + std::to_string(fd) + ", received, " +
            std::to_string(MAX_RECEIVED_LEN) + ") on fd_type " + std::to_string(int(fd_type)));

    ssize_t bytes_read = T::read(fd, received, MAX_RECEIVED_LEN);

    // If the client disconnected
    if (bytes_read == 0)
    {
        L::info(std::string("    Read 0 bytes"));

        // TODO: Assert that we reached content_length
        // TODO: Probably need to send the client a response like "expected more body bytes" if it's less than
        // content_length

        // Always true, since cgi_to_server is the only other read() caller,
        // and it raises POLLHUP rather than POLLIN on EOF, unlike client sockets
        assert(fd_type == FdType::CLIENT);

        // It's fine that this also kills the CGI
        _removeClient(client.client_fd);
        skip_client = true;

        return;
    }

    // assert(client.cgi_to_server_state != Client::CGIToServerState::DONE);
    assert(client.server_to_client_state != Client::ServerToClientState::DONE);

    L::info(std::string("    Read ") + std::to_string(bytes_read) + " bytes:\n----------\n" +
            std::string(received, bytes_read) + "\n----------\n");

    if (fd_type == FdType::CLIENT)
    {
        client.appendReadString(received, bytes_read);

        if (client.client_to_server_state == Client::ClientToServerState::DONE)
        {
            Client::RequestMethod method = client.request_method;

            size_t server_index = _getServerIndexFromClientServerName(client);
            const Config::ServerDirective &server = _config.servers.at(server_index);

            const ResolvedLocation location = _resolveToLocation(client.request_target, server.locations);

            L::info(std::string("    location.path: '") + location.path + "'");

            if (!location.resolved)
            {
                throw Client::ClientException(Status::NOT_FOUND);
            }
            if (location.has_redirect)
            {
                client.redirect = location.path;
                throw Client::ClientException(Status::MOVED_TEMPORARILY);
            }
            if (!_isAllowedMethod(location, method))
            {
                throw Client::ClientException(Status::METHOD_NOT_ALLOWED);
            }

            if (location.path.back() == '/')
            {
                if (method == Client::RequestMethod::GET)
                {
                    if (location.has_index)
                    {
                        client.respondWithFile(location.index_path);
                        _enableWritingToClient(client);
                    }
                    else if (location.autoindex)
                    {
                        client.respondWithDirectoryListing(location.path);
                        _enableWritingToClient(client);
                    }
                    else
                    {
                        throw Client::ClientException(Status::FORBIDDEN);
                    }
                }
                else
                {
                    throw Client::ClientException(Status::FORBIDDEN);
                }
            }
            else
            {
                if (std::filesystem::is_directory(location.path))
                {
                    if (method == Client::RequestMethod::DELETE)
                    {
                        throw Client::ClientException(Status::METHOD_NOT_ALLOWED);
                    }
                    else
                    {
                        throw Client::ClientException(Status::MOVED_PERMANENTLY);
                    }
                }

                if (location.is_cgi_directory)
                {
                    _startCGI(client, location.cgi_execve_path, location.path, location.path_info,
                              location.query_string);
                }
                else if (method == Client::RequestMethod::GET)
                {
                    client.respondWithFile(location.path);
                    _enableWritingToClient(client);
                }
                else if (method == Client::RequestMethod::POST)
                {
                    client.respondWithCreateFile(location.path);
                    _enableWritingToClient(client);
                }
                else
                {
                    client.respondWithDeleteFile(location.path);
                    _enableWritingToClient(client);
                }
            }
        }
    }
    else if (fd_type == FdType::CGI_TO_SERVER)
    {
        assert(client.cgi_to_server_state == Client::CGIToServerState::READING);
        L::info(std::string("    Adding this substr to the response:\n----------\n") +
                std::string(received, bytes_read) + "\n----------\n");
        client.response += std::string(received, bytes_read);
    }
    else
    {
        // TODO: Should be unreachable
        assert(false);
    }
}

void Server::_removeClient(int fd)
{
    L::info(std::string("  In _removeClient() with fd ") + std::to_string(fd));

    assert(fd != -1);

    Client &client = _getClient(fd);

    client.being_removed = true;

    // TODO: We may also need to disable other stuff?
    _disableReadingFromClient(client);

    // This if-statement is here because _reapChild() *needs* client to still exist
    // It assumes that this function gets called again after the CGI is reaped
    if (!client.cgi_killed && client.cgi_pid != -1)
    {
        _killCGI(fd);
    }
    else
    {
        L::info(std::string("  Removing client with fd ") + std::to_string(fd));

        _removeCGIToServerFd(fd);

        // TODO: Is it possible for client.client_fd to have already been closed and erased; check if it's -1?
        size_t client_index = _fd_to_client_index.at(client.client_fd);

        _fd_to_client_index[_clients.back().client_fd] = client_index;
        _fd_to_client_index[_clients.back().server_to_cgi_fd] = client_index;
        _fd_to_client_index[_clients.back().cgi_to_server_fd] = client_index;

        _removeClientFd(client.client_fd);
        _swapRemove(_clients, client_index);
    }
}

void Server::_killCGI(int fd)
{
    Client &client = _getClient(fd);

    if (!client.cgi_killed && client.cgi_pid != -1)
    {
        L::info(std::string("    Sending SIGTERM to this client's CGI script with PID ") +
                std::to_string(client.cgi_pid));

        // TODO: Isn't there a race condition here, as the cgi process may have already ended and we'll still try to
        // kill it?:
        // TODO: what happens if a zombie process is kill()ed?
        T::kill(client.cgi_pid, SIGTERM);

        client.cgi_killed = true;

        _removeCGIToServerFd(fd);
    }
}

void Server::_startCGI(Client &client, const std::string &cgi_execve_path, const std::string &script_name,
                       const std::string &path_info, const std::string &query_string)
{
    L::info(std::string("  Starting CGI..."));

    int server_to_cgi_pipe[2];
    int cgi_to_server_pipe[2];

    T::pipe(server_to_cgi_pipe);
    T::pipe(cgi_to_server_pipe);

    pid_t forked_pid = T::fork();
    if (forked_pid == CHILD)
    {
        // TODO: Comment why this was necessary
        T::signal(SIGINT, SIG_IGN);

        T::close(server_to_cgi_pipe[PIPE_WRITE_INDEX]);
        T::close(cgi_to_server_pipe[PIPE_READ_INDEX]);

        T::dup2(server_to_cgi_pipe[PIPE_READ_INDEX], STDIN_FILENO);
        T::close(server_to_cgi_pipe[PIPE_READ_INDEX]);

        T::dup2(cgi_to_server_pipe[PIPE_WRITE_INDEX], STDOUT_FILENO);
        T::close(cgi_to_server_pipe[PIPE_WRITE_INDEX]);

        _execveChild(client, cgi_execve_path, script_name, path_info, query_string);
    }

    T::close(server_to_cgi_pipe[PIPE_READ_INDEX]);
    T::close(cgi_to_server_pipe[PIPE_WRITE_INDEX]);

    client.cgi_pid = forked_pid;

    _cgi_pid_to_client_fd.emplace(forked_pid, client.client_fd);

    int server_to_cgi_fd = server_to_cgi_pipe[PIPE_WRITE_INDEX];

    size_t client_index = _fd_to_client_index.at(client.client_fd);

    if (client.server_to_cgi_state == Client::ServerToCGIState::DONE)
    {
        L::info(std::string("    Closing server_to_cgi fd immediately, since there is no body"));
        T::close(server_to_cgi_fd);
    }
    else
    {
        _addClientFd(server_to_cgi_fd, client_index, FdType::SERVER_TO_CGI, client.body.empty() ? 0 : POLLOUT);
        client.server_to_cgi_fd = server_to_cgi_fd;
        client.server_to_cgi_state = Client::ServerToCGIState::WRITING;
        _enableWritingToCGI(client);
        L::info(std::string("    Added server_to_cgi fd ") + std::to_string(server_to_cgi_fd));
    }

    int cgi_to_server_fd = cgi_to_server_pipe[PIPE_READ_INDEX];
    _addClientFd(cgi_to_server_fd, client_index, FdType::CGI_TO_SERVER, POLLIN);
    client.cgi_to_server_fd = cgi_to_server_fd;
    client.cgi_to_server_state = Client::CGIToServerState::READING;
    L::info(std::string("    Added cgi_to_server fd ") + std::to_string(cgi_to_server_fd));
}

void Server::_execveChild(Client &client, const std::string &cgi_execve_path, const std::string &script_name,
                          const std::string &path_info, const std::string &query_string)
{
    char *argv0 = const_cast<char *>(cgi_execve_path.c_str());
    char *argv1 = const_cast<char *>(script_name.c_str());
    char *argv2 = const_cast<char *>(path_info.c_str());

    char *const argv[] = {argv0, argv1, argv2, NULL};

    std::vector<std::string> cgi_headers;

    for (const auto &[key, value] : client.headers)
    {
        cgi_headers.push_back("HTTP_" + key + "=" + value);
    }

    cgi_headers.push_back("REQUEST_METHOD=" + client.getRequestMethodString());
    cgi_headers.push_back("SCRIPT_NAME=" + script_name);
    cgi_headers.push_back("PATH_INFO=" + path_info);
    cgi_headers.push_back("QUERY_STRING=" + query_string);
    cgi_headers.push_back("CONTENT_LENGTH=" + std::to_string(client.content_length));
    cgi_headers.push_back("CONTENT_TYPE=" + client.content_type);
    cgi_headers.push_back("SERVER_NAME=" + client.server_name);
    cgi_headers.push_back("SERVER_PORT=" + client.server_port);
    cgi_headers.push_back("GATEWAY_INTERFACE=CGI/1.1");
    cgi_headers.push_back("PATH_TRANSLATED=" + std::string(std::filesystem::current_path()) + path_info);
    cgi_headers.push_back("SERVER_PROTOCOL=HTTP/1.1");

    // TODO: Give some of these values?
    cgi_headers.push_back("AUTH_TYPE=");
    cgi_headers.push_back("REMOTE_ADDR=");
    cgi_headers.push_back("REMOTE_HOST=");
    cgi_headers.push_back("REMOTE_IDENT=");
    cgi_headers.push_back("REMOTE_USER=");
    cgi_headers.push_back("SERVER_SOFTWARE="); // TODO: Share this with the HTTP response?

    const auto &cgi_env_vec = _getCGIEnv(cgi_headers);
    char **cgi_env = const_cast<char **>(cgi_env_vec.data());

    T::execve(cgi_execve_path.c_str(), argv, cgi_env);
}

std::vector<const char *> Server::_getCGIEnv(const std::vector<std::string> &cgi_headers)
{
    std::vector<const char *> cgi_env;

    for (const std::string &cgi_header : cgi_headers)
    {
        cgi_env.push_back(cgi_header.c_str());
    }

    cgi_env.push_back(NULL);

    return cgi_env;
}

Server::ResolvedLocation Server::_resolveToLocation(const std::string &request_target,
                                                    const std::vector<Config::LocationDirective> &locations)
{
    ResolvedLocation resolved{};

    size_t longest_uri_length = 0;

    for (const Config::LocationDirective &location : locations)
    {
        if (Utils::startsWith(request_target, location.uri) && location.uri.length() > longest_uri_length)
        {
            longest_uri_length = location.uri.length();

            resolved.resolved = true;

            resolved.is_cgi_directory = !location.cgi_execve_path.empty();
            resolved.cgi_execve_path = location.cgi_execve_path;

            resolved.path_info = "";
            resolved.query_string = "";

            resolved.has_index = !location.index.empty();
            resolved.autoindex = location.autoindex;
            resolved.has_redirect = !location.redirect.empty();

            resolved.path = location.root + std::filesystem::weakly_canonical(request_target).string();
            resolved.index_path = "";

            resolved.get_allowed = location.get_allowed;
            resolved.post_allowed = location.post_allowed;
            resolved.delete_allowed = location.delete_allowed;

            if (resolved.has_redirect)
            {
                resolved.path = location.redirect;
            }
            else if (resolved.is_cgi_directory)
            {
                std::string unsplit_path = location.root + request_target;
                resolved.path = "";

                bool target_is_directory = true;

                size_t path_end_index = 0;
                while (path_end_index < unsplit_path.length())
                {
                    while (path_end_index < unsplit_path.length() && unsplit_path.at(path_end_index) != '/' &&
                           unsplit_path.at(path_end_index) != '?')
                    {
                        path_end_index++;
                    }

                    resolved.path = unsplit_path.substr(0, path_end_index);

                    // TODO: Remove
                    // L::debug("path_end_index: " + std::to_string(path_end_index));
                    // L::debug("resolved.path: " + resolved.path);

                    target_is_directory = std::filesystem::is_directory(resolved.path);

                    if (!target_is_directory || path_end_index >= unsplit_path.length() ||
                        unsplit_path.at(path_end_index) == '?')
                    {
                        break;
                    }

                    path_end_index++;
                }

                if (target_is_directory)
                {
                    // We want to be able to send a 301 with "/cgis/python", and a directory listing with
                    // "/cgis/python/"
                    if (resolved.path.back() != '/' && unsplit_path.back() == '/')
                    {
                        resolved.path += '/';
                    }
                }
                else
                {
                    size_t path_len = resolved.path.length();
                    size_t questionmark_index = unsplit_path.find("?", path_len);
                    // TODO: According to CGI RFC 3875 section 4.1.6. PATH_TRANSLATED we should translate "%2e" to ".",
                    // so do we want to do that?
                    resolved.path_info = unsplit_path.substr(path_len, questionmark_index - path_len);

                    resolved.query_string = unsplit_path.substr(resolved.path.length() + resolved.path_info.length());

                    L::debug("    resolved.path before resolve: " + resolved.path);

                    // Resolves "/.." to "/" to prevent escaping directories
                    resolved.path = std::filesystem::weakly_canonical(resolved.path);
                    // Resolving path_info may not be correct, I'm not sure, but it makes scripting easier
                    resolved.path_info = std::filesystem::weakly_canonical(resolved.path_info);

                    L::debug("    unsplit_path: " + unsplit_path);
                    L::debug("    resolved.path: " + resolved.path);
                    L::debug("    resolved.path_info: " + resolved.path_info);
                    L::debug("    resolved.query_string: " + resolved.query_string);
                }
            }
            else if (resolved.has_index)
            {
                resolved.index_path = resolved.path + location.index;
            }
        }
    }

    return resolved;
}

bool Server::_isAllowedMethod(const ResolvedLocation &location, Client::RequestMethod method)
{
    return (method == Client::RequestMethod::GET && location.get_allowed) ||
           (method == Client::RequestMethod::POST && location.post_allowed) ||
           (method == Client::RequestMethod::DELETE && location.delete_allowed);
}

void Server::_respondClientException(const Client::ClientException &e, Client &client)
{
    L::info(std::string("  In _respondClientException() with error: ") + e.what());

    client.status = e.status;

    _killCGI(client.client_fd);

    _disableReadingFromClient(client);

    size_t server_index = _getServerIndexFromClientServerName(client);
    const Config::ServerDirective &server = _config.servers.at(server_index);

    client.respondWithError(server.error_pages);

    _enableWritingToClient(client);
}

size_t Server::_getServerIndexFromClientServerName(const Client &client)
{
    const std::vector<size_t> &server_indices = _bind_fd_to_server_indices.at(client.server_fd);

    for (size_t inspected_server_index : server_indices)
    {
        for (const std::string &inspected_server_name : _config.servers.at(inspected_server_index).server_names)
        {
            if (inspected_server_name == client.server_name)
            {
                return inspected_server_index;
            }
        }
    }

    return server_indices.at(0);
}

void Server::_handlePollout(int fd, FdType fd_type)
{
    Client &client = _getClient(fd);

    if (fd_type == FdType::SERVER_TO_CGI)
    {
        _writeToCGI(client);
    }
    else if (fd_type == FdType::CLIENT)
    {
        _writeToClient(client, fd);
    }
    else
    {
        // TODO: Should be unreachable
        assert(false);
    }
}

void Server::_writeToCGI(Client &client)
{
    L::info(std::string("  Writing from the server to the CGI..."));

    assert(client.server_to_cgi_state == Client::ServerToCGIState::WRITING);

    size_t max_cgi_write_len = MAX_CGI_WRITE_LEN; // TODO: Read from config
    size_t body_substr_len = std::min(client.body.length() - client.body_index, max_cgi_write_len);

    assert(body_substr_len > 0);

    // TODO: substr() can fail
    std::string body_substr = client.body.substr(client.body_index, body_substr_len);

    client.body_index += body_substr_len;

    L::info(std::string("    Sending this body substr to the CGI that has a length of ") +
            std::to_string(body_substr.length()) + " bytes:\n----------\n" + body_substr + "\n----------\n");

    if (write(client.server_to_cgi_fd, body_substr.c_str(), body_substr.length()) == -1)
    {
        // Happens when the CGI script closed its stdin
        L::info(std::string("    write() detected 'Broken pipe'"));

        _removeServerToCGIFd(client.server_to_cgi_fd);

        return;
    }

    // If we don't have anything left to write
    if (client.body_index == client.body.length())
    {
        _removeServerToCGIFd(client.server_to_cgi_fd);
    }
}

void Server::_writeToClient(Client &client, int fd)
{
    L::info(std::string("  Writing to the client..."));

    assert(client.client_fd == fd);
    assert(client.server_to_client_state == Client::ServerToClientState::WRITING);

    size_t max_client_write_len = MAX_CLIENT_WRITE_LEN; // TODO: Read from config; HAS to be >= 1
    size_t response_substr_len = std::min(client.response.length() - client.response_index, max_client_write_len);

    assert(response_substr_len > 0);

    std::string response_substr = client.response.substr(client.response_index, response_substr_len);

    client.response_index += response_substr_len;

    L::info(std::string("    Sending this response substr to the client that has a length of ") +
            std::to_string(response_substr.length()) + " bytes:\n----------\n" + response_substr + "\n----------\n");

    // sleep(5); // TODO: REMOVE

    if (write(fd, response_substr.c_str(), response_substr.length()) == -1)
    {
        // Reached when `curl -v localhost:8080/tests/sent/1m_lines.txt` is cancelled halfway through
        _removeClient(client.client_fd);
        return;
    }

    // sleep(5); // TODO: REMOVE

    // TODO: Can't this remove the client before the CGI has finished
    // TODO: appending to client.response?
    // TODO: So should we check whether the cgi_to_server pipe is closed?
    if (client.response_index == client.response.length())
    {
        _removeClient(client.client_fd);
    }
}

void Server::_sigIntHandler(int signum)
{
    (void)signum;
    shutting_down_gracefully = true;
}

void Server::_sigChldHandler(int signum)
{
    (void)signum;

    static char msg[] = "In _sigChldHandler()\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);

    // TODO: throwing isn't signal safe, so change!!
    if (write(_sig_chld_pipe[PIPE_WRITE_INDEX], "!", 1) == -1)
        throw T::SystemException("write");
}
