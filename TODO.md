## Sander

- [ ] Test that any request method should be able to have a body, but that it is NEVER used for [GETs](https://stackoverflow.com/a/983458/13279557) nor [DELETEs](https://stackoverflow.com/a/299696/13279557)
- [ ] Make sure that clients can't have dangling ptrs/indices to their two CGI pipe ends
- [ ] Can GET and DELETE ever launch the CGI?
- [ ] Write test that POSTs a body with several null bytes to the CGI, expecting the null bytes *not* to the body, and for the uppercased text displayed in the browser doesn't end at the first null byte
- [ ] Every mention of "client" can dangle if the map decides to rearrange its data (growing, for example); double-check that none dangle before handing in
- [ ] Make sure all error code pages are correctly sent: 3xx is redirect; 4xx is not available; 5xx is server error
- [ ] Make sure the maps and vectors aren't growing over time with Siege
- [ ] Consider permanently fixing rare "Address already in use" by [killing any previous process and waiting till it has been reaped](https://stackoverflow.com/q/17894720/13279557)
- [ ] Make sure the server doesn't crash if the CGI script crashed
- [ ] Check that wrong request methods/request targets/protocols inputs are caught
- [ ] Lots of Server's methods take `fd`; rewrite them so they take `Client` instead, so `_getClient()` doesn't have to be called as often
- [ ] Do we want to catch `SystemException`s so we can show a prettier error message? (Does it even matter, since they should never occur?)
- [ ] Verify that all tests in `tests/config_exceptions/` throw an exception
- [ ] Verify that duplicate config keys always throw exceptions
- [ ] Make sure that `listen 127.0.0.1:80;` doesn't crash our program (since we expect a simple number like `80`)
- [ ] Verify that same behavior as nginx happens when location/server_name is missing in webserv
- [ ] Use the config's max_connections?
- [ ] Use the config's default_file?
- [ ] Ask Milan to parse something like limit_except
- [ ] Make sure the Config *requires* every Location to have a root
- [ ] Let the accesses happen relative to the config file?
- [ ] Do we want the / location to exist by default?
- [ ] Handle favicon.ico
- [ ] Add CGI script `files.py` that's going to send a response that's identical to a regular directory listing, but has garbage can emoji buttons in front of every file
- [ ] Make sure that the `/public/bar` in `http://localhost:8080/cgi-bin/files.py/public/bar/` gets sent to `files.py` so it can send a directory listing of that path when there's a GET, and can delete/create a file with POST or DELETE.
- [ ] Allow location logic to work in `cgi-bin/`, rather than having it be a special case directory. Update `README.md` appropriately.
- [ ] Hardcode default error page in C++ if opening the error status html file threw an exception by concatenating the status code and status line in between it
- [ ] Fuzz Victor's client request parser
- [ ] Talk with Marius about what is meant with "Define a HTTP redirection"; do we want to implement nginx's `return` directive?

### Config parsing

EXCEPTIONS CATCHEN OP DE JUISTE PLEKKEN

map voor (server/location) blocks om te checken of de inhoud klopt

line checks:
 - er is een '='
 - de type is valid
 - meerdere '=' tekens
 - er wordt iets in de type gezet
 - spaties tussen type, '=' en value negeren, maar spaties in de value worden niet geaccepteerd
 - een regel waarin een type gedeclared wordt moet eindigen op een ';'

DONE:
• Choose the port and host of each ’server’.
• Setup the server_names or not.
• Limit client body size.
• Setup default error pages. // finish locations

TODO:
• The first server for a host:port will be the default for this host:port (that means
it will answer to all the requests that don’t belong to an other server).

• Setup routes with one or multiple of the following rules/configuration (routes wont
be using regexp):
◦ Define a list of accepted HTTP methods for the route.
◦ Define a HTTP redirection.
◦ Define a directory or a file from where the file should be searched (for example,
if url /kapouet is rooted to /tmp/www, url /kapouet/pouic/toto/pouet is
/tmp/www/pouic/toto/pouet).
◦ Turn on or off directory listing.
◦ Set a default file to answer if the request is a directory.
◦ Execute CGI based on certain file extension (for example .php).
◦ Make it work with POST and GET methods.
◦ Make the route able to accept uploaded files and configure where they should
be saved.

# General code TODOs
- Decide whether we want to do stuff like fd closing whenever a stdlib function fails.
- Do we want to handle when the header is malformed, because it *doesn't* end with \r\n\r\n?
- Move all defines to the config
- Do we want to be fully C++98 compliant just cause?
- Make sure that when a client's request has been fully handled, all pfds get removed from the vector and maps, and that their fds get closed.
- Right now we stop reading the client if we've read everything from the CGI. Is this correct, according to the nginx behavior in practice/the HTTP 1.1 RFC? Same goes for how we stop writing to the CGI if we've read everything from the CGI.
- Detect whether the user sent a multi-part form request or not. If it is sent to a non-cgi directory, throw a client exception. Otherwise, if it is sent to ANY CGI script, just apply default behavior.
- Do we want to support non-parsed headers? See https://docstore.mik.ua/orelly/linux/cgi/ch03_03.htm#ch03-10-fm2xml
- Do we want to support having multiple Server instances active at the same time? If so, test it thoroughly
- Discuss removing Client's copy constructor and copy assignment operator, since it's a hazard that we won't even bother properly testing
- Make sure DELETE is idempotent (sending a second time has no effect)
- Consider using content_length to limit how many bytes of the body we'll try to read
- Consider enforcing a [maximum header size](https://stackoverflow.com/a/686243/13279557)
- Support custom cgi-bin directory name in the config (do we want an error or a default name if it isn't in the config?)
- Let stuff be printed to Logger class

# PDF questions
- "You can’t execve another web server." - So should we add explicit logic that throws an exception if one does try to do it? Or are they saying the program is allowed to segfault if the evaluator tries to do it?
- "Your server must never block and the client can be bounced properly if necessary." - What does bouncing of clients precisely mean? Isn't poll() technically going to block the program, or is this maybe not called blocking?
- "poll() (or equivalent) must check read and write at the same time." - Is this saying that modifying the "events" field of a pollfd struct on the fly to switch between POLLIN and POLLOUT is forbidden? Or is it just saying that a single poll() call should be used, rather than one for POLLIN, and one for POLLOUT?
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
- "Launch multiple servers at the same time with different configurations but with common ports. Does it work? If it does, ask why the server should work if one of the configurations isn't functional. Keep going." - Do they mean that webserv's a.out should be run in multiple terminals at the same time? Are they saying we should explain that if a webserv process sees a broken configuration, it should just keep chugging along? Do they *want* a warning to be thrown when two servers with the same ports but different server_names are run, cause that'd be weird since nginx handles it fine.
- "Check if there is no hanging connection." - Does Siege report this?

# Before handing in
- Consider removing PDFs and such from the repository, editing the .gitignore as well
- Test read and write sizes 1, 2, 3, 4, 5
- "Search for all read/recv/write/send and check if the returned value is correctly checked (checking only -1 or 0 values is not enough, both should be checked)." - Eval sheet
- Make sure we're not using the errno global directly: "If errno is checked after read/recv/write/send, the grade is 0 and the evaluation process ends now."
- Decide whether all of Client's copied member variables in the copy constructor and copy assignment operator make sense to be copied, or whether they should be initialized to the starting state
- Double-check that the C++ classes' initializer lists aren't forgetting to initialize a member variable
- Double-check that all the OCF methods we added work correctly in a test
- Replace as many "#include <foo.h>" with "#include <foo>" or "#include <cfoo>"
- Make sure the server socket is closed at the end of the program, along with its fds
- Make sure that maps don't grow in memory usage over time; in other words, make sure stuff is always erased
- Check for ***EVERY*** function call that its returned error value is handled properly
- Manually try crash the server by using Ctrl+C on the curl client at random intervals
- Make sure all instances of unordered_map and vector [] indexing are replaced by .at() or .emplace() or .find(); see [this](https://devblogs.microsoft.com/oldnewthing/20190227-00/?p=101072) blog post on why [] is evil in C++.
- Consider letting the `make all` turn off asserts, turning them on when the user does `make all DEBUG=1`
- Replace any .insert(std::make_pair()) with .emplace()
- Are we allowed to use exit()? I think it depends on the interpretation of "Everything in C++ 98."? If not, replace all mentions of exit()
- Make sure that even a length of 1 works for all the #defines, like MAX_CLIENT_WRITE_LEN
- Remove all functions that aren't signal handler safe from the signal handlers
- Make sure that a body in a GET or a DELETE *is* parsed, [but not handled further](https://stackoverflow.com/a/983458/13279557)
- Make sure `_printContainerSizes()` prints *every* Server map and vector, and that none of them grow over time
- Make sure no async-unsafe calls like printing are done in any signal handlers (including `Server::_sigChldHandler()`)
- Replace all perror() and exit() calls with proper exiting logic
- Check that `curl -X POSTS --data-binary @tests/2_lines.txt localhost:18000` gives the client an error, due to the trailing `S` in `POSTS`
- Remove every reference to the transitionary `UNDECIDED` `Status` enum
- Using `#pragma once` in every header
- PLace `const` at the end of prototypes wherever possible to indicate `this` is `const`
- Make sure *every* `Status` enum has an associated string in `status_text_table`
