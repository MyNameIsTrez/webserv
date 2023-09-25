import json, os, sys, time


def main():
    print("This is printed to stderr by print.py", file=sys.stderr)

    # TODO: Test that the server doesn't crash if the script crashes

    # Non-parsed header
    # print("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 1\r\n\r\nab")

    print(f"argv: {sys.argv}")

    # TODO: Make separate test for closing stdin
    # TODO: This previously caused "Broken pipe", but doesn't anymore. How to get it to do it again? Can this error can happen when this script exits and the C++ code immediately writes to it?
    # os.close(sys.stdin.fileno())
    # sys.stdin.close()
    # print("Disabled stdin in print.py", file=sys.stderr)

    print("Before reading stdin", file=sys.stderr)
    print(f"stdin: {sys.stdin.readlines()}")
    print("After reading stdin", file=sys.stderr)

    print(f"env: {json.dumps(dict(os.environ), sort_keys=True, indent=4)}")

    print("", end="", flush=True)

    os.close(sys.stdout.fileno())
    sys.stdout.close()

    # TODO: Probably want to comment this out before handing in
    time.sleep(5)

    # TODO: Test that this sends a "CGI failed" page to the client
    exit(2)


if __name__ == "__main__":
    main()
