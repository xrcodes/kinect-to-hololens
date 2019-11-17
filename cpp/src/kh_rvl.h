#pragma once

#include <vector>

// This algorithm is from
// Wilson, A. D. (2017, October). Fast lossless depth image compression.
// In Proceedings of the 2017 ACM International Conference on Interactive Surfaces and Spaces (pp. 100-105). ACM.
namespace kh
{
namespace rvl
{
// Compresses depth pixels using RVL.
std::vector<char> compress(short* input, int num_pixels);
// Decompress depth pixels using RVL.
std::vector<short> decompress(char* input, int num_pixels);
}
}