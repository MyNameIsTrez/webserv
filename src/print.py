import json, os, sys, time


def main():
    print("This is printed to stderr by print.py", file=sys.stderr)

    print(f"argv: {sys.argv}")

    # TODO: Test closing stdin

    print(f"stdin: {sys.stdin.readlines()}")

    print(f"env: {json.dumps(dict(os.environ), sort_keys=True, indent=4)}")

    print("", end="", flush=True)

    os.close(sys.stdout.fileno())
    sys.stdout.close()

    time.sleep(5)

    # TODO: Test that this sends a "CGI failed" page to the client
    exit(2)


if __name__ == "__main__":
    main()
