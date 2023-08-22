# Remaining tasks

- [ ] GET request method response
- [ ] POST request method response
- [ ] DELETE request method response
- [ ] CGI script launching
- [ ] Chunked requests
- [ ] Our own tests?

## Sander

- [x] Set up regular NGINX server
- [ ] Run tester on NGINX server

## Victor

- [ ] Incoming request parsing (using class that caches reading/writing progress?)

## Milan

- [ ] Main webserv while-loop that uses poll()
- [ ] NGINX's configuration format parser


# PDF questions
- How are the provided "tester" and "cgi_tester" executables supposed to be used?
- "You can’t execve another web server." - So should we add explicit logic that throws an exception if one does try to do it? Or are they saying the program is allowed to segfault if the evaluator tries to do it?
- "Your server must never block and the client can be bounced properly if necessary." - How do I make sure that reads and writes are non-blocking? What does bouncing of clients precisely mean?
- "Because you have to use non-blocking file descriptors, it is possible to use read/recv or write/send functions with no poll() (or equivalent), and your server wouldn’t be blocking. But it would consume more system resources." - What would exactly happen to the system resources?
- "A request to your server should never hang forever." - How do we test this?
- "Your server must have default error pages if none are provided." - What do they mean by "if none are provided"? Are they saying that some pages *have* to have different looking error pages?
- "You must be able to serve a fully static website." - Is there anything we could accidentally do that would make it *not* static? Would adding a single 'X' character to a sent file count as dynamic, since it was not in the file? So does every URL path need to be fully static, or just at least one of them? And what exactly differentiates static from dynamic?
- "Setup the server_names or not." - What is meant by "or not"?
- "The first server for a host:port will be the default for this host:port (that means it will answer to all the requests that don’t belong to an other server)." - So if ordering matters, does that mean it doesn't make sense to do the configuration file in JSON?
- "Because you won’t call the CGI directly, use the full path as PATH_INFO." - What do they mean by us not calling the CGI directly? Which full path? And by "as PATH_INFO", do they mean that we should put the full path in PATH_INFO, or take it out of it?
- "Your program should call the CGI with the file requested as first argument." - By "with the file", are they referring to foo.py if the client requests "localhost:18000/foo.py"? And by "as first argument", do they mean that "foo.py" should be passed into the argv of the execve() call that runs foo.py?
- "The CGI should be run in the correct directory for relative path file access." - By "should be run", do they mean that the C++ code shall not run a .py file outside of cgi-bin/ ? Or are they saying that if the Python script tries to open a file, it should be relative to itself, rather than the webserv executable?
- "Do not test with only one program. Write your tests with a more convenient language such as Python or Golang, and so forth. Even in C or C++ if you want to." - Are they saying that we're not allowed to have all the tests be inside the webserv C++ code, even when ifdef guarded?

# Eval sheet questions
- "There should be only one read or one write per client per select() (or equivalent). Ask the group to show you the code from the select() (or equivalent) to the read and write of a client." - How could there be multiple reads per client per select()? Are they not referring to the read and write args of a select() call? Or are they saying that a client isn't allowed to both be registered in select in the read and write groups at the same time? Or are they referring to read() and write() calls that may occur after a select() call returns?
- "Writing or reading ANY file descriptor without going through the select() (or equivalent) is strictly FORBIDDEN." - Can we presume it isn't forbidden in client.c?
- "Setup multiple servers with different hostnames (use something like: curl --resolve example.com:80:127.0.0.1 http://example.com/)." - What does using resolve have to do with different hostnames? Do we even have a hostname if we always use localhost?
- "Limit the client body (use: curl -X POST -H "Content-Type: plain/text" --data "BODY IS HERE write something shorter or longer than body limit")." - What is meant by "limit" here? And where is the body limit defined? Add these commands to the tests once figured out.
- "Setup a default file to search for if you ask for a directory." - Do they mean that there should be a config file that whitelists which directories are accessible to clients?
- "Setup a list of methods accepted for a certain route (e.g., try to delete something with and without permission)." - Is having permission determined by the permissions the user running the server has? Or are all clients to be treated as having a limited set of permissions? Or are we supposed to give some clients more permissions than others?
- "Using telnet, curl, prepared files, demonstrate that the following
features work properly:" - Do we have to brew install telnet, or can we just use curl instead?
- "For every test you should receive the appropriate status code." - Which status code? Do they mean that the server has to send the appropriate status code?
- "You need to test with files containing errors to see if the error handling works properly." - This is under the CGI header in the eval sheet, but is this referring to either the server return()ing a proper error to the terminal, or is it referring to sending a proper error to the client? And does "with files" refer to CGI files?
- "Use the reference browser of the team. Open the network part of it, and try to connect to the server using it." - What do they mean by "try to conncet to the server using it"? Do they just mean going to the URL with the browser's address bar?
- "It should be compatible to serve a fully static website." - WTF do they mean with "compatible"?
- "In the configuration file setup multiple ports and use different websites." - Do they want the web server to edit /etc/hosts or /private/etc/hosts so the user is redirected to our 127.0.0.1 server when they go to for example foo.com? Or do they want the server to be running on different IPs than just 127.0.0.1?
- "Launch multiple servers at the same time with different configurations but with common ports. Does it work? If it does, ask why the server should work if one of the configurations isn't functional. Keep going." - Do they mean that webserv's a.out should be run in multiple terminals at the same time? Are they saying we should explain that if a webserv process sees a broken configuration, it should just keep chugging along?
- "Check if there is no hanging connection." - Does Siege report this?

# Decide whether these clients should be redirected to different pages
{
	port 9000
	host localhost
}
{
	port 9000
	host f1r3s6
}
{
	port 9000
	host 127.0.0.1
}
{
	port 9000
	host f1r3s6.codam.nl
}

# Before handing in
- Consider editing .gitignore
- "Search for all read/recv/write/send and check if the returned value is correctly checked (checking only -1 or 0 values is not enough, both should be checked)." - Eval sheet
- Make sure we're not using the errno global directly: "If errno is checked after read/recv/write/send, the grade is 0 and the evaluation process ends now."
