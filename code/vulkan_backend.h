#if !defined(VULKAN_BACKEND_H)
#define VULKAN_BACKEND_H

#if defined (_WIN32)
#define VK_PLATFORM_SURFACE "VK_KHR_win32_surface"
#elif defined (__linux__)
#define VK_PLATFORM_SURFACE "VK_KHR_xcb_surface"
#endif

typedef struct {
    VkInstance                  instance;
    VkSurfaceKHR                surface;
    VkDevice                    device;
    VkPhysicalDevice            phys_device;
    VkQueue                     graphics_queue;
    VkQueue                     present_queue;
    VkFormat                    image_format;
    VkColorSpaceKHR             color_space;
    VkPresentModeKHR            present_mode;
} Vulkan_State;

#endif