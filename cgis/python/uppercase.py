import sys


def main():
    print("Content-Type: text/html")
    print()
    print("".join(sys.stdin.readlines()).upper())


if __name__ == "__main__":
    main()
