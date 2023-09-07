import json, os, sys


def main():
    print("This is printed to stderr by print.py", file=sys.stderr)

    print(f"argv: {sys.argv}")

    print(f"stdin: {sys.stdin.readlines()}")

    print(f"env: {json.dumps(dict(os.environ), sort_keys=True, indent=4)}")

    # TODO: Test that this sends a "CGI failed" page to the client
    exit(2)


if __name__ == "__main__":
    main()
