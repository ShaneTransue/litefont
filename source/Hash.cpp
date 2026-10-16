#include <litefont/Hash.h>

namespace litefont {

std::uint64_t Hash::fnv1a(std::span<const std::uint8_t> data) {
	constexpr std::uint64_t offsetBasis = 14695981039346656037ull;
	return Hash::fnv1a(data, offsetBasis);
}

std::uint64_t Hash::fnv1a(std::span<const std::uint8_t> data, std::uint64_t basis) {
	constexpr std::uint64_t prime = 1099511628211ull;

	std::uint64_t hash = basis;
	for (const std::uint8_t byte : data) {
		hash ^= byte;
		hash *= prime;
	}
	return hash;
}

}
