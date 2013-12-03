#include "../common/random.hpp"

#include <iostream>

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        while (true)
            std::cout << random_::generate() << '\n';
    }
    else
    {
        int max = atoi(argv[1]);
        while (true)
            std::cout << random_::to(max) << '\n';
    }
}
