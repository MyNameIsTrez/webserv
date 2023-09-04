import json, os, sys


def main():
    print(f"argv: {sys.argv}")

    print(f"stdin: {sys.stdin.readlines()}")

    print(f"env: {json.dumps(dict(os.environ), sort_keys=True, indent=4)}")

    exit(2)


if __name__ == "__main__":
    main()
