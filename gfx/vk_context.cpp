#include "vk_context.hpp"

#include <SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <set>
#include <stdexcept>

#include "SDL_video.h"
#include "logging.hpp"

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation",
};
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

namespace rdm::gfx::vk {
static VKAPI_ATTR VkBool32 VKAPI_CALL
__VKDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                  void* pUserData) {
  Log::printf(LOG_DEBUG, "Vulkan msg:\n%s", pCallbackData->pMessage);
  return VK_FALSE;
}

VKContext::VKContext(void* hwnd) : BaseContext(hwnd) {
  window = (SDL_Window*)hwnd;

  Log::printf(LOG_WARN, "Vulkan support is experimental");

#ifndef NDEBUG
  useValidation = true;
#else
  useValidation = false;
#endif

  const VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                     NULL,
                                     "",
                                     VK_MAKE_VERSION(1, 0, 0),
                                     "RDM4001",
                                     VK_MAKE_VERSION(1, 0, 0),
                                     VK_API_VERSION_1_0};

  std::vector<const char*> extensions = getRequiredExtensions();

  Log::printf(LOG_DEBUG, "Vulkan instance extensions:");
  for (int i = 0; i < extensions.size(); i++) {
    Log::printf(LOG_DEBUG, "  %s", extensions[i]);
  }

  VkInstanceCreateInfo instInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                   NULL,
                                   0,
                                   &appInfo,
                                   0,
                                   NULL,
                                   static_cast<uint32_t>(extensions.size()),
                                   extensions.data()};

  if (useValidation && checkValidationSupport()) {
    instInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    instInfo.ppEnabledLayerNames = validationLayers.data();
  } else if (useValidation) {
    throw std::runtime_error(
        "Validation requested, but validation layers are not available");
  }

  VkResult result = vkCreateInstance(&instInfo, NULL, &vkInst);

  if (result != VK_SUCCESS) {
    Log::printf(LOG_ERROR, "VkResult = %i", result);
    throw std::runtime_error("VKContext could not create vkInstance");
  }

  SDL_Vulkan_CreateSurface(window, vkInst, &surface);

  uint32_t pdCount;
  vkEnumeratePhysicalDevices(vkInst, &pdCount, NULL);
  vkPhysicalDevices.resize(pdCount);
  vkEnumeratePhysicalDevices(vkInst, &pdCount, vkPhysicalDevices.data());

  physicalDevice = VK_NULL_HANDLE;

  for (const auto& device : vkPhysicalDevices) {
    if (isDeviceSuitable(device)) {
      physicalDevice = device;
      break;
    }
  }

  if (physicalDevice == VK_NULL_HANDLE) {
    Log::printf(LOG_ERROR, "Could not find any suitable devices (tested %i)",
                vkPhysicalDevices.size());
    throw std::runtime_error("VKContext couldn't find any suitable devices");
  }

  QmIndices indices = findQueueFamilies(physicalDevice);

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                            indices.presentFamily.value()};

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures deviceFeatures{};

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.queueCreateInfoCount =
      static_cast<uint32_t>(queueCreateInfos.size());

  createInfo.pEnabledFeatures = &deviceFeatures;
  createInfo.enabledExtensionCount = 0;

  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = deviceExtensions.data();

  if (useValidation) {
    createInfo.enabledLayerCount =
        static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
  } else {
    createInfo.enabledLayerCount = 0;
  }

  if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create logical device!");
  }

  vkGetDeviceQueue(logicalDevice, indices.graphicsFamily.value(), 0,
                   &graphicsQueue);
  vkGetDeviceQueue(logicalDevice, indices.presentFamily.value(), 0,
                   &presentQueue);

  createSwapChain();
}

VKContext::~VKContext() {
  for (size_t i = 0; i < swapChainImageViews.size(); i++) {
    vkDestroyImageView(logicalDevice, swapChainImageViews[i], NULL);
  }
  vkDestroySwapchainKHR(logicalDevice, swapChain, NULL);
  vkDestroyDevice(logicalDevice, nullptr);
}

void VKContext::createSwapChain() {
  VKSwapChainSupport supportDetails = querySwapChainSupport(physicalDevice);
  QmIndices indices = findQueueFamilies(physicalDevice);

  VkSwapchainCreateInfoKHR swCreateInfo{};
  swCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swCreateInfo.surface = surface;
  swCreateInfo.minImageCount = supportDetails.capabilities.minImageCount + 1;
  swCreateInfo.imageFormat = supportDetails.preferedFormat.format;
  swCreateInfo.imageColorSpace = supportDetails.preferedFormat.colorSpace;
  swCreateInfo.imageExtent = supportDetails.swapExtent;
  swCreateInfo.imageArrayLayers = 1;
  swCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  uint32_t qfnIndices[] = {indices.graphicsFamily.value(),
                           indices.presentFamily.value()};
  if (indices.graphicsFamily != indices.presentFamily) {
    swCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swCreateInfo.queueFamilyIndexCount = 2;
    swCreateInfo.pQueueFamilyIndices = qfnIndices;
  } else {
    swCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swCreateInfo.queueFamilyIndexCount = 0;
    swCreateInfo.pQueueFamilyIndices = NULL;
  }
  swCreateInfo.preTransform = supportDetails.capabilities.currentTransform;
  swCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swCreateInfo.presentMode = supportDetails.preferedPresentMode;
  swCreateInfo.clipped = VK_TRUE;
  swCreateInfo.oldSwapchain = VK_NULL_HANDLE;
  VkResult result =
      vkCreateSwapchainKHR(logicalDevice, &swCreateInfo, NULL, &swapChain);
  if (result != VK_SUCCESS) {
    Log::printf(LOG_ERROR, "Swapchain result: %i", result);
    throw std::runtime_error("vkCreateSwapchainKHR failed");
  }

  swapChainImageFormat = swCreateInfo.imageFormat;
  swapChainExtent = swCreateInfo.imageExtent;

  uint32_t imageCount;
  vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, NULL);
  swapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount,
                          swapChainImages.data());

  if (swapChainImages.size()) {
    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
      vkDestroyImageView(logicalDevice, swapChainImageViews[i], NULL);
    }
  }

  swapChainImageViews.resize(imageCount);
  for (size_t i = 0; i < imageCount; i++) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = swapChainImages[i];
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = swapChainImageFormat;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(logicalDevice, &createInfo, NULL,
                          &swapChainImageViews[i]) != VK_SUCCESS) {
      throw std::runtime_error("Error creating image views");
    }
  }
}

QmIndices VKContext::findQueueFamilies(VkPhysicalDevice device) {
  QmIndices indices;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           queueFamilies.data());

  int i = 0;
  for (const auto& queueFamily : queueFamilies) {
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = i;
    }

    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
    if (presentSupport) {
      indices.presentFamily = i;
    }

    i++;
  }

  return indices;
}

VKSwapChainSupport VKContext::querySwapChainSupport(VkPhysicalDevice device) {
  VKSwapChainSupport details;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                            &details.capabilities);

  if (details.capabilities.currentExtent.width != UINT32_MAX) {
    details.swapExtent = details.capabilities.currentExtent;
  } else {
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    VkExtent2D actualExtent = {static_cast<uint32_t>(width),
                               static_cast<uint32_t>(height)};
    actualExtent.width = std::clamp(actualExtent.width,
                                    details.capabilities.minImageExtent.width,
                                    details.capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(
        actualExtent.height, details.capabilities.minImageExtent.height,
        details.capabilities.maxImageExtent.height);
  }

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, NULL);
  if (formatCount) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                         details.formats.data());

    details.preferedFormat = details.formats[0];
    for (const auto& availableFormat : details.formats) {
      if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
          availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        details.preferedFormat = availableFormat;
      }
    }
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                            NULL);
  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface, &presentModeCount, details.presentModes.data());

    details.preferedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& availablePresent : details.presentModes) {
      if (availablePresent == VK_PRESENT_MODE_MAILBOX_KHR) {
        details.preferedPresentMode = availablePresent;
      }
    }
  }

  return details;
}

bool VKContext::isDeviceSuitable(VkPhysicalDevice device) {
  QmIndices indices = findQueueFamilies(device);
  VKSwapChainSupport support = querySwapChainSupport(device);
  if (!indices.complete()) return false;
  if (!checkDeviceExtensions(device)) return false;
  if (support.formats.empty()) return false;
  if (support.presentModes.empty()) return false;
  return true;
}

bool VKContext::checkDeviceExtensions(VkPhysicalDevice device) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);
  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount,
                                       availableExtensions.data());

  std::set<std::string> requiredExtensions(deviceExtensions.begin(),
                                           deviceExtensions.end());
  for (const auto& extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

std::vector<const char*> VKContext::getRequiredExtensions() {
  uint32_t extensionCount;
  SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, NULL);
  std::vector<const char*> extensions(extensionCount);
  SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data());
  if (useValidation) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
  return extensions;
}

bool VKContext::checkValidationSupport() {
  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, NULL);
  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
  Log::printf(LOG_DEBUG, "Vulkan validation layers:");
  for (const auto& layerProperties : availableLayers) {
    Log::printf(LOG_DEBUG, "  %s (%s)", layerProperties.layerName,
                layerProperties.description);
  }
  for (const char* layerName : validationLayers) {
    bool layerFound = false;
    for (const auto& layerProperties : availableLayers) {
      if (strcmp(layerName, layerProperties.layerName) == 0) {
        Log::printf(LOG_DEBUG, "Found validation layer %s", layerName);
        layerFound = true;
        break;
      }
    }
    if (!layerFound) {
      Log::printf(LOG_ERROR, "Could not find validation layer %s", layerName);
      return false;
    }
  }
  return true;
}

void VKContext::setCurrent() {}

void VKContext::swapBuffers() {}

void VKContext::unsetCurrent() {}

glm::ivec2 VKContext::getBufferSize() {
  return glm::ivec2(swapChainExtent.width, swapChainExtent.height);
}
}  // namespace rdm::gfx::vk
