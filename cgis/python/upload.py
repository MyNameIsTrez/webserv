"""This file handles multi-part form requests."""

import os
import sys
from io import BytesIO

import multipart as mp


def main():
    # request_method = "POST"
    request_method = os.environ.get("REQUEST_METHOD")
    if request_method != "POST":
        print("Content-Type: text/html")
        print("Status: 405 Method Not Allowed")
        print()
        exit(0)

    # content_type = "multipart/form-data; boundary=----WebKitFormBoundaryBRGOgydgtRaDx2Ju"
    content_type = os.environ.get("HTTP_CONTENT_TYPE")

    # print(f"content_type: {content_type}", file=sys.stderr)

    # with open("manual_multi_form_request.txt") as f:
    #     data = f.buffer

    data = sys.stdin.buffer

    forms, files = mp.parse_form_data(
        {
            "REQUEST_METHOD": "POST",
            "CONTENT_TYPE": content_type,
            "wsgi.input": data,
        },
        strict=True,
    )

    # print("forms:", file=sys.stderr)
    # for i, form in enumerate(forms):
    #     print(f"[{i}]: {form}", file=sys.stderr)

    # path_translated = "/home/sbos/Programming/webserv/public/"
    path_translated = os.environ.get("PATH_TRANSLATED")

    for name, file in files.iterallitems():
        filename = file.filename
        value = file.value

        # print(
        #     f"name {name} with filename {filename}: '{value}'",
        #     file=sys.stderr,
        # )

        path = path_translated + filename
        try:
            with open(path, "x") as f:
                f.write(value)
        except FileExistsError as e:
            print(f"Failed to open file at path {path}", file=sys.stderr)

            print("Content-Type: text/html")
            print("Status: 400 Bad Request")
            print()

            return

    print("Content-Type: text/plain")
    print()
    print("Uploaded file")


if __name__ == "__main__":
    main()
