#if !defined(VULKAN_BACKEND_H)
#define VULKAN_BACKEND_H

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