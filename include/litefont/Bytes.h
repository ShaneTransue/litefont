#pragma once

#include <cstddef>
#include <cstdint>

#include <vector>

namespace litefont {

/**
 * \struct Bytes Bytes.h <litefont/Bytes.h>
 * @addtogroup IO
 *
 * Bytes - An owned block of bytes and nothing more. Objects made from bytes
 * (a Font, later an Image) keep one as a member; ByteReader and ByteWriter
 * work on the data, and Hash::fnv1a hashes it.
 */
struct Bytes {
	std::vector<std::uint8_t> data;
};

}
