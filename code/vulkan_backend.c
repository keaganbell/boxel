
bool vulkan_backend_create_instance(Vulkan_State *vk, const char **layers, int layer_count, const char **extensions, int extension_count) {
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.enabledLayerCount = layer_count;
    create_info.ppEnabledLayerNames = layers;
    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = extensions;
    if (vkCreateInstance(&create_info, NULL, &vk->instance) != VK_SUCCESS) {
        print_error("Failed to create Vulkan instance");
        return false;
    }
    return true;
}

bool vulkan_backend_create_surface(Vulkan_State *vk, void *data) {
#if defined(_WIN32)

    VkWin32SurfaceCreateInfoKHR info = {0};
    info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    info.hwnd = (HWND)data;
    info.hinstance = GetModuleHandle(0);
    vkCreateWin32SurfaceKHR(vk->instance, &info, 0, &vk->surface);
    return true;

#elif defined(__linux__)
    return false;
#endif
}

bool vulkan_backend_check_device_extensions(Fixed_Arena *arena, VkPhysicalDevice device, const char **extensions, uint32_t count) {
    Scratch_Arena scratch = arena_begin_scratch(arena);
    bool result = true;
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, 0, &extension_count, 0);
    VkExtensionProperties *properties = push_array(scratch.arena, VkExtensionProperties, extension_count);
    vkEnumerateDeviceExtensionProperties(device, 0, &extension_count, properties);
    for (uint32_t i = 0; i < count; ++i) {
        bool found = false;
        for (uint32_t j = 0; j < extension_count; ++j) {
            if (strcmp(extensions[i], properties[j].extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) result = false;
    }
    arena_end_scratch(&scratch);
    return result;
}

bool vulkan_backend_check_present_mode(Fixed_Arena *arena, VkPhysicalDevice device, VkSurfaceKHR surface, VkPresentModeKHR mode) {
    Scratch_Arena scratch = arena_begin_scratch(arena);
    bool result = false;
    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, 0);
    VkPresentModeKHR *present_modes = push_array(scratch.arena, VkPresentModeKHR, present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, present_modes);
    for (uint32_t i = 0; i < present_mode_count; ++i) {
        if (present_modes[i] == mode) {
            result = true;
            break;
        }
    }
    arena_end_scratch(&scratch);
    return result;
}

bool vulkan_backend_check_formats(Fixed_Arena *arena, VkPhysicalDevice device, VkSurfaceKHR surface, VkFormat format, VkColorSpaceKHR color_space) {
    Scratch_Arena scratch = arena_begin_scratch(arena);
    bool result = false;
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, 0);
    VkSurfaceFormatKHR *formats = push_array(scratch.arena, VkSurfaceFormatKHR, format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, formats);
    for (uint32_t i = 0; i < format_count; ++i) {
        if (formats[i].format == format && formats[i].colorSpace == color_space) {
            result = true;
            break;
        }
    }
    arena_end_scratch(&scratch);
    return result;
}

bool vulkan_backend_select_device(Vulkan_State *vk, Fixed_Arena *arena) {

    if (!vk->surface) {
        print_error("Failed to select device. No surface specified.");
        return false;
    }

    bool result = false;
    Scratch_Arena scratch = arena_begin_scratch(arena);

    int device_count = 0;
    vkEnumeratePhysicalDevices(vk->instance, &device_count, 0);
    VkPhysicalDevice *phys_devices = push_array(scratch.arena, VkPhysicalDevice, device_count);
    vkEnumeratePhysicalDevices(vk->instance, &device_count, phys_devices);

    const char *device_extensions[] = { "VK_KHR_swapchain" };

    VkPhysicalDevice best_device = 0;
    for (int device_idx = 0; device_idx < device_count; ++device_idx) {
        VkPhysicalDevice candidate = phys_devices[device_idx];

        int queue_family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_family_count, 0);
        VkQueueFamilyProperties *queue_properties = push_array(scratch.arena, VkQueueFamilyProperties, queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_family_count, queue_properties);

        int graphics_index = -1;
        int present_index = -1;
        VkBool32 present_support = false;
        for (int family_idx = 0; family_idx < queue_family_count; ++family_idx) {
            present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(candidate, family_idx, vk->surface, &present_support);
            if (queue_properties[family_idx].queueFlags &VK_QUEUE_GRAPHICS_BIT)
                graphics_index = family_idx;
            if (present_support)
                present_index = family_idx;
            if (graphics_index != -1 && present_index != -1)
                break;
        }

        bool has_queues = graphics_index != -1 && present_index != -1;
        bool has_extensions = vulkan_backend_check_device_extensions(arena, candidate, device_extensions, array_count(device_extensions));
        bool has_formats = vulkan_backend_check_formats(arena, candidate, vk->surface, VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
        bool has_present_mode = vulkan_backend_check_present_mode(arena, candidate, vk->surface, VK_PRESENT_MODE_MAILBOX_KHR);
        if (present_support && has_queues && has_extensions && has_formats && has_present_mode) {
            float queue_priority = 1.0f;

            VkDeviceQueueCreateInfo queue_create_info = {0};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = graphics_index;
            queue_create_info.queueCount = 1;
            queue_create_info.pQueuePriorities = &queue_priority;

            VkPhysicalDeviceFeatures device_features = {};
            device_features.samplerAnisotropy = VK_TRUE;
            
            VkDeviceCreateInfo device_create_info = {};
            device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_create_info.pQueueCreateInfos = &queue_create_info;
            device_create_info.queueCreateInfoCount = 1;
            device_create_info.pEnabledFeatures = &device_features;
            device_create_info.ppEnabledExtensionNames = device_extensions;
            device_create_info.enabledExtensionCount = array_count(device_extensions);

            if (vkCreateDevice(candidate, &device_create_info, 0, &vk->device) == VK_SUCCESS) {
                vk->phys_device = candidate;
                vkGetDeviceQueue(vk->device, graphics_index, 0, &vk->graphics_queue);
                vkGetDeviceQueue(vk->device, present_index, 0, &vk->present_queue);
                vk->image_format = VK_FORMAT_B8G8R8A8_SRGB;
                vk->color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
                vk->present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
                result = true;
            }

        }

    }
    arena_end_scratch(&scratch);
    return result;
}

bool vulkan_backend_create_swapchain(Vulkan_State *vk, int width, int height) {
    return true;
}

bool vulkan_backend_init(Vulkan_State *vk, Fixed_Arena *scratch, void *window, int width, int height) {
    if (volkInitialize() != VK_SUCCESS) {
        print_error("Volk failed to initialize.");
        return false;
    }

    const char *layers[] = { "VK_LAYER_KHRONOS_validation" };
    const char *extensions[] = { "VK_KHR_surface", "VK_KHR_win32_surface" };
    if (!vulkan_backend_create_instance(vk, layers, array_count(layers), extensions, array_count(extensions))) return false;
    volkLoadInstance(vk->instance);

    if (!vulkan_backend_create_surface(vk, window)) return false;
    if (!vulkan_backend_select_device(vk, scratch)) return false;
    if (!vulkan_backend_create_swapchain(vk, width, height)) return false;

    return true;
}
