#include <iostream>
#include <variant>

// clear && c++ variant.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined -std=c++2b && ./a.out
int main()
{
    std::variant<int, short> v = (short)12;

    if (std::holds_alternative<int>(v))
    {
        std::cout << "Int: " << std::get<int>(v) << std::endl;
    }

    if (std::holds_alternative<short>(v))
    {
        std::cout << "Short: " << std::get<short>(v) << std::endl;
    }
}
