"""This file sends a fancy directory listing containing trash can buttons."""


import sys


def main():
	print("Content-Type: text/plain")
	print()

	print(f"PATH_INFO: {sys.argv[1]}")

	# "		<button onclick=\"requestDelete(this.nextSibling.href)\">🗑</button><a href=\"public/foo.txt\">foo.txt</a><br>\n"


if __name__ == "__main__":
	main()
