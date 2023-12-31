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
- [PATH_INFO and QUERY_STRING](https://en.wikipedia.org/wiki/Common_Gateway_Interface#Deployment)

## Setting up multiple servers with different hostnames

This will print example.com's response:
`curl http://example.com/`

This will print localhost's response: (by search-and-replacing example.com)
`curl --resolve example.com:8080:127.0.0.1 http://example.com:8080/`

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

- Benchmark a URL indefinitely with `siege -b http://localhost:8080`
- Siege a URL with `c` clients repeated `r` times with `siege -c2 -r3 http://localhost:8080`
- Let 5 clients send a POST request sending `a` with `siege -c5 -r1 '127.0.0.1:8080/cgis/python/uppercase.py POST </home/sbos/Programming/webserv/public/a.html'`
- Let 5 clients send a POST request sending `1k_lines.txt` with `siege -c5 -r1 '127.0.0.1:8080/cgis/python/uppercase.py POST </home/sbos/Programming/webserv/tests/sent/1k_lines.txt'`
- Let 10 clients send 10 GET requests to the CGI script `debug.py` with `siege -c10 -r10 http://localhost:8080/cgis/python/debug.py`

## Memory usage

- `valgrind --tool=massif ./webserv`
- `ms_print massif.out.* > mem.log`

### Notes

- Sometimes `siege http://localhost:8080` prints `[error] socket: unable to connect sock.c:282: Operation timed out`, which is expected [according to the author of siege](https://github.com/JoeDog/siege/issues/176#issuecomment-1274215687), as siege may overload the web server

## Running Codam's tester

- First run `export REQUEST_METHOD="GET" SERVER_PROTOCOL="HTTP/1.1" PATH_INFO="/foo/bar"`
- Then run `./cgi_tester`, and press Enter once

## Using curl

- GET: `curl localhost:8080`
- POST: `curl --data-binary @tests/1_line.txt localhost:8080`
- POST chunked: `curl -H "Transfer-Encoding: chunked" --data-binary @tests/1_line.txt localhost:8080`
- POST with newline trimming: `curl -d @tests/1_line.txt localhost:8080`
- DELETE: `curl -X DELETE localhost:8080`
- Nonexistent header type: `curl -X FOO localhost:8080`
- Start CGI script `debug.py`: `curl http://localhost:8080/cgis/python/debug.py`
- Check whether the CGI is still running: `ps -aux | grep print.py`
- Check who is causing "Address already in use": `netstat -tulpn | grep 8080`
- Create `foo.txt` containing two "foo"s: `yes foo | dd of=foo.txt count=2 bs=4`
- Create `stack_overflow.json` containing repeated `{"":`: `yes '{"":' | dd of=stack_overflow.json count=420420 bs=5`
- POST a file containing 10k lines: `curl --data-binary @tests/10k_lines.txt localhost:8080`

## Using nc

- `echo "GET /foo/../foo/a.html" | nc localhost 8080`
- `nc -l 8081` start nc server
- `echo "GET /foo/../foo/a.html" | nc localhost 8081` connect with nc server

## Fuzzing the config parser

1. Run `docker build -t aflplusplus-webserv fuzzing && docker run --rm -it -v .:/src aflplusplus-webserv` to build and run docker
2. Run `setup.sh` to compile for afl-cmin + afl-tmin, generate tests, and compile for AFL
2. Run `FUZZ_CLIENT=1 setup.sh` to do the same but fuzzing the client instead of the config.
3. Run `coverage.sh` to fuzz while generating coverage
3. Run `FUZZ_CLIENT=1 coverage.sh` to do the same but fuzzing the client instead of the config.
3. Run `AFL_DEBUG=1 FUZZ_CLIENT=1 fuzz.sh` to get debug information in case it crashes.
4. Run `minimize_crashes.sh` to minimize the crashes, which are then put in `/src/fuzzing/afl/minimized-crashes/`

### Debugging main_fuzzing_client.cpp after running setup.sh
`clear && AFL_DEBUG=1 afl-fuzz -D -i /src/fuzzing/tests_fuzzing_client -o /src/fuzzing/afl/afl-output -M master -- /src/fuzzing/fuzzing_ctmin /src/fuzzing/fuzzing_client_master_webserv.json`

### Compiling main_fuzzing_client.cpp manually in the fuzzing Docker container
`clear && g++ -Wall -Wextra -Werror -Wfatal-errors -Wshadow -Wswitch -Wimplicit-fallthrough -Wno-c99-designator -Werror=type-limits -std=c++2b -DSUPPRESS_LOGGING src/config/Config.cpp src/config/JSON.cpp src/config/Node.cpp src/config/Tokenizer.cpp src/Client.cpp src/Logger.cpp src/Server.cpp src/Throwing.cpp src/Utils.cpp fuzzing/src/main_fuzzing_client.cpp; echo foo | ./a.out /src/fuzzing/fuzzing_client_master_webserv.json`

`clear && dict_path=/src/fuzzing/conf_fuzzing_client.dict; AFL_DEBUG=1 afl-fuzz -D -i /src/fuzzing/afl/trimmed-tests -x $dict_path -o /src/fuzzing/afl/afl-output -M master -- /src/fuzzing/fuzzing_afl /src/fuzzing/fuzzing_client_master_webserv.json 2> /src/fuzzing/master.log`

### Fuzzing with multiple cores
`FUZZ_CLIENT=1 fuzz.sh`
`ps aux > foo.log`
`clear && afl-whatsup /src/fuzzing/afl/afl-output/`

## Manual multipart request submission
`clear && cat manual_multi_form_request.txt | REQUEST_METHOD=POST HTTP_CONTENT_TYPE='multipart/form-data; boundary=----WebKitFormBoundaryBRGOgydgtRaDx2Ju' python3 cgis/python/upload.py`

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
server_index = get_server_index_from_client_server_name(client)
server = servers[server_index]

location = resolve_location(target, server)

if not location.resolved:
	raise NOT_FOUND

if location.has_redirect:
	client.redirect = location.path
	raise MOVED_TEMPORARILY

if not is_allowed_method(location, method):
	raise METHOD_NOT_ALLOWED

if location.path[-1] == '/:
	if method == "GET":
		if location.has_index:
			client.respond_with_file(location.index_path)
			enable_writing_to_client(client)
		elif location.autoindex:
			client.respond_with_directory_listing(location.path)
			enable_writing_to_client(client)
		else
			raise FORBIDDEN
	else:
		raise FORBIDDEN
else:
	if is_directory(location.path):
		if method == "DELETE":
			raise METHOD_NOT_ALLOWED
		else:
			raise MOVED_PERMANENTLY

	if location.is_cgi_directory:
		start_cgi(client, location.cgi_settings, location.path, location.path_info, location.query_string)
	elif method == "GET":
		respond_with_file(location.path)
		enable_writing_to_client(client)
	elif method == "POST":
		create_file(location.path)
		enable_writing_to_client(client)
	else:
		delete_file(location.path)
		enable_writing_to_client(client)
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
