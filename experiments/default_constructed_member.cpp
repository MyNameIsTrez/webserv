#include <cstdlib>
#include <iostream>
#include <string>

struct Inner
{
	Inner(void) {
		std::cerr << "Inside Inner" << std::endl;
	}

	std::string name;
	int b;
};

struct Outer
{
	int a;
	Inner inner;
};

void recurse(int depth)
{
	if (depth == 0) return;

	Outer outer;
	// Outer outer{}; // Zero-initializes a and inner.b

	// a and b are random memory!
	// name is *NOT* random memory, since it is a default constructed string!
	std::cerr
		<< "a: " << outer.a
		<< ", inner.b: " << outer.inner.b
		<< ", inner.name: '" << outer.inner.name << "'"
		<< std::endl;

	recurse(depth - 1);
}

// c++ default_constructed_member.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
int main(void)
{
	recurse(50);

	return EXIT_SUCCESS;
}
