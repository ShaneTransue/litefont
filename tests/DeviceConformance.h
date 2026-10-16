#pragma once

#include <litefont/GraphicsDevice.h>
#include "TestKit.h"
#include "RecordingDevice.h"

#include <filesystem>

namespace litefont::tests {

/**
 * \class DeviceConformance DeviceConformance.h
 *
 * DeviceConformance - The checks every GraphicsDevice runs: a fixed scene
 * (a texel-exact quad, a half-pixel quad, a scaled quad, a textured triangle
 * with alpha, and a clipped batch) drawn and compared with a golden image:
 * exactly for SoftwareDevice; GPUs round filter weights where the scene
 * samples between texels, so they get two levels there.
 */
class DeviceConformance {
public:
	static Image draw(GraphicsDevice& device);
	static void run(test::Context& context, GraphicsDevice& device, const std::filesystem::path& golden, std::int32_t tolerance = 0);
};

}
