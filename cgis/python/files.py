"""This file sends a fancy directory listing that has a file upload button and trash can buttons."""

import os
import sys
from pathlib import Path

import multipart as mp


def main():
    # request_method = "POST"
    request_method = os.environ.get("REQUEST_METHOD")
    if request_method == "GET":
        get()
    elif request_method == "POST":
        post()
    else:
        print("Content-Type: text/html")
        print("Status: 405 Method Not Allowed")
        print()
        exit(0)


def get():
    print("Content-Type: text/html")
    print()

    path_translated = os.environ.get("PATH_TRANSLATED")
    path_info = os.environ.get("PATH_INFO")

    p = Path(path_translated)

    print(
        """
<!DOCTYPE html>
<html>
    <meta charset="UTF-8">
	<head>
        <title>"""
        + "Index of "
        + path_info
        + "</title>"
        + """
		<style>
			body {
				background-color: gray;
				font-size: large;
			}
			a {
				padding-left: 2px;
				margin-left: 4px;
			}
			button {
				background-color: gray;
				padding-left: 3px;
				padding-right: 3px;
				border: 0px;
				margin: 2px;
				cursor: pointer;
			}
            hr {
                border-color: black;
                background-color: black;
                color: black;
            }
		</style>
		<script>
			function requestDelete(file) {
                // console.log(`file: ${file}`);
				const response = fetch(file, {
					method: "DELETE",
				}).then((response) => {
                    if (response.ok) {
                        location.reload();
                    } else {
                        throw new Error("Failed to delete the file.");
                    }
				}).catch((error) => {
                    alert("Failed to delete the file.");
                });
			}
		</script>
	</head>
	<body>
		<form method="POST" enctype="multipart/form-data" id="Form">
			<input name="file" type="file" multiple/>
			<input type="submit" onclick="console.log('foo'); location.reload();"/>
		</form>
        <h1>Index of """
        + path_info
        + "</h1>"
        "		<hr>"
        "       <pre>"
    )

    print('<a href="../">../</a>')

    directories = list(entry for entry in p.iterdir() if entry.is_dir())
    directories.sort()
    for directory in directories:
        print(f'<a href="{directory.name}/">{directory.name}/</a>')

    files = list(entry for entry in p.iterdir() if entry.is_file())
    files.sort()
    for file in files:
        name = file.name
        printed = f'<button onclick="requestDelete(this.nextSibling.href)">🗑</button><a href="{path_info}{name}">{name}</a>'
        print(printed)

    print(
        """</pre>
        <hr>
	</body>
</html>
"""
    )


def post():
    # content_type = "multipart/form-data; boundary=----WebKitFormBoundaryBRGOgydgtRaDx2Ju"
    content_type = os.environ.get("HTTP_CONTENT_TYPE")

    # print(f"content_type: {content_type}", file=sys.stderr)

    forms, files = mp.parse_form_data(
        {
            "REQUEST_METHOD": "POST",
            "CONTENT_TYPE": content_type,
            "wsgi.input": sys.stdin.buffer,
        },
        charset="utf-8",
        strict=True,
    )

    # print("forms:", file=sys.stderr)
    # for i, form in enumerate(forms):
    #     print(f"[{i}]: {form}", file=sys.stderr)

    # path_translated = "/home/sbos/Programming/webserv/public/"
    path_translated = os.environ.get("PATH_TRANSLATED")

    for name, file in files.iterallitems():
        filename = file.filename
        value = file.raw

        # print(
        #     f"name {name} with filename {filename}: '{value}'",
        #     file=sys.stderr,
        # )

        path = path_translated + filename
        try:
            with open(path, "xb") as f:
                f.write(value)
        except FileExistsError as e:
            print(f"Failed to open file at path {path}", file=sys.stderr)

            print("Content-Type: text/html")
            print("Status: 400 Bad Request")
            print()

            return

    print("Content-Type: text/html")
    print("Status: 205 Reset Content")
    print()
    print("Uploaded file")


if __name__ == "__main__":
    main()
