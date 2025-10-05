#pragma once
#include <SDL2/SDL.h>
#include <vulkan/vulkan_core.h>

#include <optional>
#include <vector>

#include "base_context.hpp"
namespace rdm::gfx::vk {
struct QmIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;
  bool complete() {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};

struct VKSwapChainSupport {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;

  VkSurfaceFormatKHR preferedFormat;
  VkPresentModeKHR preferedPresentMode;
  VkExtent2D swapExtent;
};

class VKContext : public BaseContext {
  friend class VKDevice;

  SDL_Window* window;

  VkInstance vkInst;
  VkSwapchainKHR swapChain;
  std::vector<VkImage> swapChainImages;
  std::vector<VkImageView> swapChainImageViews;
  VkFormat swapChainImageFormat;
  VkExtent2D swapChainExtent;

  std::vector<VkPhysicalDevice> vkPhysicalDevices;
  std::vector<VkQueueFamilyProperties> vkQueueFamilies;
  VkPhysicalDevice physicalDevice;
  VkDevice logicalDevice;
  VkSurfaceKHR surface;

  int graphicsQueueIndex;

  bool useValidation;

  bool checkValidationSupport();
  std::vector<const char*> getRequiredExtensions();

  void createSwapChain();
  bool isDeviceSuitable(VkPhysicalDevice device);
  bool checkDeviceExtensions(VkPhysicalDevice device);
  QmIndices findQueueFamilies(VkPhysicalDevice device);
  VKSwapChainSupport querySwapChainSupport(VkPhysicalDevice device);

  VkQueue graphicsQueue;
  VkQueue presentQueue;

 public:
  VKContext(void* hwnd);
  virtual ~VKContext();

  virtual void setCurrent();
  virtual void swapBuffers();
  virtual void unsetCurrent();
  virtual glm::ivec2 getBufferSize();
};
}  // namespace rdm::gfx::vk
