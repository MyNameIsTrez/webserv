# webserv

## Used links

- [Siege](https://www.joedog.org/siege-home/) (HTTP load tester)
- [RFC key word explanations](https://www.rfc-editor.org/rfc/pdfrfc/rfc2119.txt.pdf)
- [RFC key word explanations clarification](https://www.rfc-editor.org/rfc/pdfrfc/rfc8174.txt.pdf)
- [HTTP response status codes](https://developer.mozilla.org/en-US/docs/Web/HTTP/Status#client_error_responses)
- [webserv eval sheet](https://rphlr.github.io/42-Evals/Rank05/webserv/)
- [CGI programming is simple!](http://www.whizkidtech.redprince.net/cgi-bin/tutorial)
- [CGI C examples](https://www.eskimo.com/~scs/cclass/handouts/cgi.html)
- [NGINX configuration guide](https://www.plesk.com/blog/various/nginx-configuration-guide/)

## Setting up multiple servers with different hostnames

This will print example.com's response:
`curl http://example.com/`

This will print localhost's response: (by search-and-replacing example.com)
`curl --resolve example.com:18000:127.0.0.1 http://example.com:18000/`
