#pragma once

#include <cstddef>
#include <cstdint>

#include <span>

namespace litefont {

/**
 * \class Hash Hash.h <litefont/Hash.h>
 * @addtogroup Core
 *
 * Hash - Content hashes that are stable across platforms and runs, so they can
 * key caches (a Font's id is the hash of its file). Passing a previous hash
 * as the basis continues it over more data.
 */
class Hash {
public:
	static std::uint64_t fnv1a(std::span<const std::uint8_t> data);
	static std::uint64_t fnv1a(std::span<const std::uint8_t> data, std::uint64_t basis);
};

}
