#pragma once

#include "Yoru/Core/Log.h"

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <glm/glm.hpp>

#include <vector>
#include <memory>
#include <format>
#include <functional>
#include <optional>
#include <deque>

//struct VkInstance_T;
//struct VkPhysicalDevice_T;
//struct VkDevice_T;
//struct VkQueue_T;
//struct VkDescriptorPool_T;
//struct VkCommandBuffer_T;
//struct VkShaderModule_T;
////struct VkDebugUtilsMessengerEXT_T;
////struct VkDebugUtilsMessengerCreateInfoEXT_T;
//using VkInstance = VkInstance_T*;
//using VkPhysicalDevice = VkPhysicalDevice_T*;
//using VkDevice = VkDevice_T*;
//using VkQueue = VkQueue_T*;
//using VkDescriptorPool = VkDescriptorPool_T*;
//using VkCommandBuffer = VkCommandBuffer_T*;
//using VkShaderModule = VkShaderModule_T*;
////using VkDebugUtilsMessengerEXT = VkDebugUtilsMessengerEXT_T*;
////using VkDebugUtilsMessengerCreateInfoEXT = VkDebugUtilsMessengerCreateInfoEXT_T*;
//enum VkFormat;
//struct VkAllocationCallbacks;

struct GLFWwindow;
struct VmaAllocator_T;
struct VmaAllocation_T;
struct VmaAllocationInfo;
using VmaAllocator = VmaAllocator_T*;
using VmaAllocation = VmaAllocation_T*;


namespace Yoru
{
	constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 2;

#define VK_CHECK(x)																				   \
	do																							   \
	{																							   \
		VkResult err = x;																		   \
		if (err)																				   \
		{																						   \
			Log::Write(LogLevel::FATAL, "VK_CHECK {}", string_VkResult(err));					   \
			abort();																			   \
		}																						   \
	} while (0)

	struct DeletionQueue
	{
		std::deque<std::function<void()>> Deletors;

		void PushFunction(std::function<void()>&& function)
		{
			Deletors.push_back(function);
		}

		void Flush()
		{
			// reverse iterate the deletion queue to execute all the functions
			for (auto it = Deletors.rbegin(); it != Deletors.rend(); it++)
			{
				(*it)(); //call functors
			}

			Deletors.clear();
		}
	};

	struct FrameData
	{
		VkCommandPool CommandPool;
		VkCommandBuffer CommandBuffer;
		// Wait till we get ImageFromSwapchain
		// (RenderSemaphore lives per swapchain image in VulkanRenderer)
		VkSemaphore SwapchainSemaphore;
		// Wait till gpu has rendered to prevent overwriting gpu commands
		VkFence RenderFence;
		DeletionQueue FrameDeletionQueue;
		//DescriptorAllocatorDynamic FrameDescriptors;
	};

	struct AllocatedImage
	{
		VkImage Image;
		VkImageView ImageView;
		VmaAllocation Allocation;
		VkExtent3D ImageExtent;
		VkFormat ImageFormat;
	};

	struct AllocatedBuffer
	{
		VkBuffer Buffer;
		VmaAllocation Allocation;
		VmaAllocationInfo* Info;
	};

	struct GPUMeshBuffers
	{
		AllocatedBuffer IndexBuffer;
		AllocatedBuffer VertexBuffer;
		VkDeviceAddress VertexDeviceAddress;
	};

	struct GPUDrawPushConstants
	{
		glm::mat4 WorldMatrix;
		VkDeviceAddress VertexBufferAddress;
		glm::vec2 Padding;
		glm::vec4 OverrideColor;
	};

	struct GPUSceneData
	{
		glm::mat4 View;
		glm::mat4 Proj;
		glm::mat4 ViewProj;
		glm::vec4 AmbientColor = glm::vec4(glm::vec3(0.03f), 1.0f);
		glm::vec4 CameraPosition;
		float Time;
		glm::vec3 Padding;
	};

	struct alignas(16) Light
	{
		glm::vec3 Position = { 0.0f, -1.0, 0.0f };
		// 0 is PointLight, 1 is SpotLight, 2 is DirectionalLight
		int Type = 0;
		glm::vec3 Direction = { 0.0f, -1.0, 0.0f };
		float Intensity = 1.0f;
		glm::vec4 Color = glm::vec4(1.0f);
		glm::mat4 LightProj{};
	};

	struct alignas(16) LightData
	{
		int Count = 0;
		int TotalPointLights = 0;
		int TotalSpotLights = 0;
		int TotalDirectionalLights = 0;
		std::vector<Light> Lights;
	};

	enum class MaterialPass : uint8_t
	{
		MainColor,
		Transparent,
		Other
	};

	struct MaterialPipeline
	{
		VkPipeline Pipeline;
		VkPipelineLayout Layout;
	};

	struct MaterialInstance
	{
		MaterialPipeline* Pipeline;
		VkDescriptorSet MaterialSet;
		MaterialPass PassType;
	};

	struct GLTFMetallic_Roughness
	{
		MaterialPipeline OpaquePipeline;
		MaterialPipeline TransparentPipeline;

		VkDescriptorSetLayout MaterialLayout;

		// 256 bytes in total because good default alignment
		struct MaterialConstants
		{
			glm::vec4 ColorFactors;
			glm::vec4 MetalRoughFactors;
			// padding, may need for uniform buffer
			glm::vec4 Extra[14];
		};

		struct MaterialResources
		{
			AllocatedImage ColorImage;
			VkSampler ColorSampler;
			AllocatedImage MetalRoughImage;
			VkSampler MetalRoughSampler;
			AllocatedImage AOImage;
			VkSampler AOSampler;
			AllocatedImage NormalMapImage;
			VkSampler NormalMapSampler;
			VkBuffer DataBuffer;
			uint32_t DataBufferOffset;
		};

		//DescriptorWriter Writer;

		void BuildPipelines(class VKRenderer* engine);
		void ClearResources(VkDevice device);
		//MaterialInstance WriteMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorDynamic& descriptorAllocator);
	};

	struct DrawContext;

	class IRenderable
	{
		virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
	};

	struct Node : public IRenderable
	{
		void RefreshTransform(const glm::mat4& parentMatrix)
		{
			WorldTransform = parentMatrix * LocalTransform;
			for (auto& c : Children)
			{
				c->RefreshTransform(WorldTransform);
			}
		}

		virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx)
		{
			for (auto& c : Children)
			{
				c->Draw(topMatrix, ctx);
			}
		}

		std::weak_ptr<Node> Parent;
		std::vector<std::shared_ptr<Node>> Children;

		glm::mat4 LocalTransform;
		glm::mat4 WorldTransform;
	};

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> GraphicsFamily = std::nullopt;
		std::optional<uint32_t> PresentFamily = std::nullopt;
		bool IsComplete()
		{
			return GraphicsFamily.has_value() && PresentFamily.has_value();
		}
	};

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR Capabilities;
		std::vector<VkSurfaceFormatKHR> Formats = {};
		std::vector<VkPresentModeKHR> PresentModes = {};
	};

	struct VulkanDeviceContext
	{
		VkInstance Instance = VK_NULL_HANDLE;
		VkSurfaceKHR Surface = VK_NULL_HANDLE;
		GLFWwindow* Window = nullptr;
		VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
		VkDevice Device = VK_NULL_HANDLE;
		VkQueue GraphicsQueue = VK_NULL_HANDLE;
		VkQueue PresentQueue = VK_NULL_HANDLE;
		uint32_t GraphicsQueueFamily = 0;
		VmaAllocator Allocator = VK_NULL_HANDLE;
	};
}
