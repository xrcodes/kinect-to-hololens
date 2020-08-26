#pragma once

#include <vector>
#include <gsl/gsl>

// This algorithm is from
// Wilson, A. D. (2017, October). Fast lossless depth image compression.
// In Proceedings of the 2017 ACM International Conference on Interactive Surfaces and Spaces (pp. 100-105). ACM.
namespace kh
{
namespace rvl
{
// It has to be int16_t not uint16_t to work with TRVL.
std::vector<std::byte> compress(gsl::span<const std::int16_t> input, int num_pixels);
std::vector<std::int16_t> decompress(gsl::span<const std::byte> input, int num_pixels) noexcept;
}
}