"""This file sends a fancy directory listing containing trash can buttons."""

import os
from pathlib import Path


def main():
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
                console.log(`file: ${file}`);
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
			function updateAction() {
                const path = window.location.pathname.replace("files.py", "upload.py");
				document.getElementById("Form").setAttribute("action", path);
			}
		</script>
	</head>
	<body>
		<form method="POST" enctype="multipart/form-data" id="Form">
			<input name="file" type="file" location="/public/" multiple/>
			<input type="submit" onclick="updateAction()"/>
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


if __name__ == "__main__":
    main()
