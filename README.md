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

## Setting up multiple servers with different hostnames

This will print example.com's response:
`curl http://example.com/`

This will print localhost's response: (by search-and-replacing example.com)
`curl --resolve example.com:18000:127.0.0.1 http://example.com:18000/`

## Running nginx

- Download nginx with `brew install nginx`
- Start nginx with `nginx`
- Go to `http://localhost:8080/` to view the default nginx page
- View the help menu with `nginx -h`
- Open the configuration file with `code /Users/sbos/.brew/etc/nginx/nginx.conf`
- Reload configuration file without closing connections with `nginx -s reload`
- Check whether nginx is running with `ps aux | grep nginx`
- Stop nginx with `pkill nginx`

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
- POST with newline trimming: `curl -d @tests/1_line.txt localhost:18000`
- DELETE: `curl -X DELETE localhost:18000`
- Nonexistent header type: `curl -X FOO localhost:18000`
- Check whether the CGI is still running: `ps -aux | grep print.py`
- Check who is causing "Address already in use": `netstat -tulpn | grep 18000`
- Create a `foo.txt` file containing two "foo"s: `yes foo | dd of=foo.txt count=2 bs=4`
- POST a file containing 10k lines: `curl --data-binary @tests/10k_lines.txt localhost:18000`
