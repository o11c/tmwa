#include "mt_rand.hpp"

std::mt19937 mt_random = std::mt19937(std::random_device()());
