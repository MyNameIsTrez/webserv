# webserv

## Used links

- [Siege](https://www.joedog.org/siege-home/) (HTTP load tester)
- [RFC key word summary](https://mtsknn.fi/blog/rfc-2119-in-a-nutshell/)
- [HTTP response status codes](https://developer.mozilla.org/en-US/docs/Web/HTTP/Status#client_error_responses)
- [webserv eval sheet](https://rphlr.github.io/42-Evals/Rank05/webserv/)
- [CGI programming is simple!](http://www.whizkidtech.redprince.net/cgi-bin/tutorial)
- [CGI C examples](https://www.eskimo.com/~scs/cclass/handouts/cgi.html)
- [NGINX configuration guide](https://www.plesk.com/blog/various/nginx-configuration-guide/)
- [See comments on this answer](https://stackoverflow.com/a/5616108/13279557) about how non-blocking is impossible
- [Non-blocking I/O with regular files](https://www.remlab.net/op/nonblock.shtml)
- [File operations always block](https://stackoverflow.com/a/56403228/13279557)
- [Non-blocking disk I/O](https://neugierig.org/software/blog/2011/12/nonblocking-disk-io.html)
- [NGINX configuration format explanation](https://stackoverflow.com/a/46918583/13279557)
- [NGINX "location" directive explanation](https://www.digitalocean.com/community/tutorials/nginx-location-directive#1-nginx-location-matching-all-requests)
- [Full example NGINX configuration](https://www.nginx.com/resources/wiki/start/topics/examples/full/)
- [How NGINX processes a request](https://nginx.org/en/docs/http/request_processing.html)
- [How NGINX redirection and try_files works](https://serverfault.com/a/1094257/1055398)
- [HTTP header status flowchart](https://upload.wikimedia.org/wikipedia/commons/8/88/Http-headers-status.png)

## Setting up multiple servers with different hostnames

This will print example.com's response:
`curl http://example.com/`

This will print localhost's response: (by search-and-replacing example.com)
`curl --resolve example.com:18000:127.0.0.1 http://example.com:18000/`

- Go to default (first) `server` in NGINX with `curl --resolve example.com:8080:127.0.0.1 http://example.com:8080/`
- Go to `server_name` "f1r4s8" in NGINX with `curl http://f1r4s8:8080`
- Go to `server_name` "f1r4s8.codam.nl" in NGINX with `curl http://f1r4s8.codam.nl:8080`

## Running nginx

- Build and run docker with `docker build -t nginx nginx/ && docker run --rm -it -v $(pwd):/code -p 8080:80 -p 8081:81 nginx`
- Start nginx with `nginx`
- View the help menu with `nginx -h`
- Reload configuration file without closing connections with `nginx -s reload`
- View the website with `http://localhost:8080/`
- View the nginx log with `cat /var/log/nginx/access.log`

## Running siege

- Siege a URL with `c` clients repeated `r` times with `siege -c2 -r3 http://localhost:8080`

### Notes

- Sometimes `siege http://localhost:8080` prints `[error] socket: unable to connect sock.c:282: Operation timed out`, which is expected [according to the author of siege](https://github.com/JoeDog/siege/issues/176#issuecomment-1274215687), as siege may overload the web server

## Running Codam's tester

# TODO: Figure out wtf we're supposed to do with this tester

- First run `export REQUEST_METHOD="GET" SERVER_PROTOCOL="HTTP/1.1" PATH_INFO="/foo/bar"`
- Then run `./cgi_tester`, and press Enter once

## Using curl

- GET: `curl localhost:18000`
- POST: `curl --data-binary @tests/1_line.txt localhost:18000`
- POST chunked: `curl -H "Transfer-Encoding: chunked" --data-binary @tests/1_line.txt localhost:18000`
- POST with newline trimming: `curl -d @tests/1_line.txt localhost:18000`
- DELETE: `curl -X DELETE localhost:18000`
- Nonexistent header type: `curl -X FOO localhost:18000`
- Check whether the CGI is still running: `ps -aux | grep print.py`
- Check who is causing "Address already in use": `netstat -tulpn | grep 18000`
- Create a `foo.txt` file containing two "foo"s: `yes foo | dd of=foo.txt count=2 bs=4`
- POST a file containing 10k lines: `curl --data-binary @tests/10k_lines.txt localhost:18000`

## Using nc

- `echo "GET /foo/../foo/a.html" | nc localhost 8080`
- `nc -l 8081` start nc server
- `echo "GET /foo/../foo/a.html" | nc localhost 8081` connect with nc server

## Fuzzing the config parser

1. Run `docker build -t aflplusplus-webserv fuzzing && docker run --rm -it -v .:/src aflplusplus-webserv` to build and run docker
2. Run `setup.sh` to compile for afl-cmin + afl-tmin, generate tests, and compile for AFL
3. Run `coverage.sh` to fuzz while generating coverage
4. Run `minimize_crashes.sh` to minimize the crashes, which are then put in `/src/fuzzing/afl/minimized-crashes/`

## Plan of attack

Assuming `root /code/public;`, with this file tree:
```
public/
	foo/
		a.png
		b.html
		c.mp4
	d.html
```

```py
target = sanitize_request_target(target)

# nginx just outright closes the connection
if port_to_server_index.find(port) == port_to_server_index.end():
	raise NOT_FOUND

server_index = port_to_server_index.at(port)
server = servers.at(server_index)

location = resolve_location(target, server)

if not is_allowed_method(location, method):
	raise METHOD_NOT_ALLOWED

if target.ends_with("/"):
	if method == "GET":
		if location.has_index:
			respond_with_file(location.path)
		elif location.autoindex:
			respond_with_directory_listing(location.path)
		else
			raise FORBIDDEN
	else:
		raise FORBIDDEN
else:
	struct stat status
	if stat(location.path, &status) != -1 and status.type == DIRECTORY:
		if method == "DELETE":
			raise METHOD_NOT_ALLOWED
		else:
			raise MOVED_PERMANENTLY
	elif method == "GET":
		if target.starts_with("/cgi-bin/"):
			start_cgi(target)
		else:
			respond_with_file(location.path)
	elif method == "POST":
		create_file(location.path)
	else:
		delete_file(location.path)
```

```c++
// Construct this by looping over all servers and looping over their ports,
// letting the key be the server's `server_name` + ":" + `listen` port.
// If the key was already present, throw a config exception.
//
// This map is then used once we've fully read a client's header
// to map a Host header like `f1r4s8:8080` to a virtual server, saving it in the client
// If the Host header isn't in this map, use _port_to_default_server_index
std::unordered_map<std::string, size_t> _http_host_header_to_server_index;

// Construct this by looping over all servers and looping over their ports,
// letting the key be the server's `listen` port.
// If the key was already present, don't overwrite it, and just continue.
//
// This map is used as a fallback for _http_host_header_to_server_index
// The `port` is gotten by substr()-ing `8080` from `f1r4s8:8080` from the Host header
std::unordered_map<std::string, size_t> _port_to_default_server_index;

// Construct an unordered_set of all port numbers
uint16_t port;
if (!parseNumber(key, &port)) throw InvalidLineException();
std::unordered_set<uint16_t> _port_numbers;
_port_numbers.emplace(port);

// Use the unordered_set to call bind for every port number
port_fd = socket(AF_INET, SOCK_STREAM, 0);
servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
servaddr.sin_port = htons(port);
bind(port_fd, (sockaddr *)&servaddr, sizeof(servaddr));

// Done when a port_fd has POLLIN
int client_fd = accept(port_fd, NULL, NULL);

// The client's request_target is finally used to find the location directive
// in their virtual server
```
