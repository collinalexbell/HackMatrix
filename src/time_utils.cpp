#include "time_utils.h"
#include <chrono>

const auto programStart =
    std::chrono::steady_clock::now();

double nowSeconds()
{
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now() - programStart).count();
}

