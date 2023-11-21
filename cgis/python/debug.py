import json
import os
import sys
import time


def main():
    print("This is printed to stderr by debug.py", file=sys.stderr)

    # TODO: This is causing a broken-pipe error, so comment this back in!
    # when the client immediately does Ctrl+C after running `siege -b 127.0.0.1:8080/cgis/python/debug.py`
    # time.sleep(1)

    print("Content-Type: text/plain")
    print("Status: 400 Wow")
    print()

    print(f"argv: {sys.argv}")

    print(f"stdin: {sys.stdin.readlines()}")

    print(f"env: {json.dumps(dict(os.environ), sort_keys=True, indent=4)}")

    print("", end="", flush=True)


if __name__ == "__main__":
    main()
