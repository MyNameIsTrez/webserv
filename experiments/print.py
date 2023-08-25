import sys


def main():
    print(f"argv: {sys.argv}")

    print(f"stdin: {sys.stdin.readlines()}")

    exit(2)


if __name__ == "__main__":
    main()
