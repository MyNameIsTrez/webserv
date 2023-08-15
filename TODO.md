# Tasks

- [ ] Make sure multiple clients can read files without blocking

# Eval sheet questions
- "There should be only one read or one write per client per select() (or equivalent). Ask the group to show you the code from the select() (or equivalent) to the read and write of a client." - How could there be multiple reads per client per select()? Are they not referring to the read and write args of a select() call? Or are they saying that a client isn't allowed to both be registered in select in the read and write groups at the same time? Or are they referring to read() and write() calls that may occur after a select() call returns?
- "Writing or reading ANY file descriptor without going through the select() (or equivalent) is strictly FORBIDDEN." - Can we presume it isn't forbidden in client.c?
- "Setup multiple servers with different hostnames (use something like: curl --resolve example.com:80:127.0.0.1 http://example.com/)." - What does using resolve have to do with different hostnames? Do we even have a hostname if we always use localhost?
- "Limit the client body (use: curl -X POST -H "Content-Type: plain/text" --data "BODY IS HERE write something shorter or longer than body limit")." - What is meant by "limit" here? And where is the body limit defined? Add these commands to the tests once figured out.
- "Setup a default file to search for if you ask for a directory." - Do they mean that there should be a config file that whitelists which directories are accessible to clients?
- "Setup a list of methods accepted for a certain route (e.g., try to delete something with and without permission)." - Is having permission determined by the permissions the user running the server has? Or are all clients to be treated as having a limited set of permissions? Or are we supposed to give some clients more permissions than others?

# Eval sheet TODOs
- "Search for all read/recv/write/send and check if the returned value is correctly checked (checking only -1 or 0 values is not enough, both should be checked)."

# Before handing in
- Consider editing .gitignore
- Make sure we're not using the errno global directly: "If errno is checked after read/recv/write/send, the grade is 0 and the evaluation process ends now."
