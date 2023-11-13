import json, os, sys, time


def main():
    print("This is printed to stderr by debug.py", file=sys.stderr)

    print("Content-Type: text/plain")
    print("Status: 400 Wow")
    print("", end="\n")
    print("xd")
    # exit(1)
    return

    print("Content-Type: foo")

    if os.environ.get("HTTP_CONTENT_LENGTH"):
        print(f"HTTP_CONTENT_LENGTH: {os.environ.get('HTTP_CONTENT_LENGTH')}", file=sys.stderr)
    else:
        # TODO: Should this be unreachable? It's always reached and printing to stderr rn
        print("HTTP_CONTENT_LENGTH is not in env", file=sys.stderr)

    # TODO:
    # print("Content-Length: 1\r\n\r\nab")

    # Add body separator
    print("", end="\n")

    print(f"argv: {sys.argv}")

    # TODO: Make this a separate test for closing stdin
    # TODO: RFC 3875, section 4.2: "Therefore, the script MUST NOT attempt to read more than CONTENT_LENGTH bytes, even if more data is available. However, it is not obliged to read any of the data."
    # os.close(sys.stdin.fileno())
    # sys.stdin.close()
    # print("Disabled stdin in debug.py", file=sys.stderr)

    # time.sleep(5)

    # TODO: Handle the error when sys.stdin.readlines() is done after stdin is closed
    print("Before reading stdin", file=sys.stderr)
    print(f"stdin: {sys.stdin.readlines()}")
    print("After reading stdin", file=sys.stderr)

    # print("xd" * 1000000)

    print(f"env: {json.dumps(dict(os.environ), sort_keys=True, indent=4)}")

    print("", end="", flush=True)

    os.close(sys.stdout.fileno())
    sys.stdout.close()

    # TODO: Probably want to comment this out before handing in
    time.sleep(1)

    # exit(1) # Causes "500 Internal Server Error"

    exit(0)


if __name__ == "__main__":
    main()
