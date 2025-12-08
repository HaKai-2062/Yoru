#pragma once

#include <vulkan/vulkan.h>
#include <string_view>

namespace Yoru
{
	class VKContext;
	namespace VKUtils
	{
		void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, uint32_t mipLevels = VK_REMAINING_MIP_LEVELS, uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS);
		void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destinatidon, VkExtent2D srcSize, VkExtent2D dstSize);
		void generateMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
		bool loadCubeMap(VKContext* engine, std::string_view filename, VkFormat format);
	}
}