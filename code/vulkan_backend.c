
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
    XCB_Window *xcb = (XCB_Window *)data;
    VkXcbSurfaceCreateInfoKHR info = {0};
    info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    info.connection = xcb->connection;
    info.window = xcb->handle;
    if (vkCreateXcbSurfaceKHR(vk->instance, &info, NULL, &vk->surface) != VK_SUCCESS) return false;
    return true;
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

bool vulkan_backend_select_device(Vulkan_State *vk, Fixed_Arena *arena, VkFormat image_format, VkColorSpaceKHR color_space, VkPresentModeKHR present_mode) {

    if (!vk->surface) {
        print_error("Failed to select device. No surface specified.");
        return false;
    }

    bool result = false;
    Scratch_Arena scratch = arena_begin_scratch(arena);

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(vk->instance, &device_count, 0);
    VkPhysicalDevice *phys_devices = push_array(scratch.arena, VkPhysicalDevice, device_count);
    vkEnumeratePhysicalDevices(vk->instance, &device_count, phys_devices);

    const char *device_extensions[] = { "VK_KHR_swapchain" };

    for (uint32_t device_idx = 0; device_idx < device_count; ++device_idx) {
        VkPhysicalDevice candidate = phys_devices[device_idx];

        uint32_t queue_family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_family_count, 0);
        VkQueueFamilyProperties *queue_properties = push_array(scratch.arena, VkQueueFamilyProperties, queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_family_count, queue_properties);

        uint32_t graphics_index = UINT32_MAX;
        uint32_t present_index = UINT32_MAX;
        VkBool32 present_support = false;
        for (uint32_t family_idx = 0; family_idx < queue_family_count; ++family_idx) {
            present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(candidate, family_idx, vk->surface, &present_support);
            if (queue_properties[family_idx].queueFlags &VK_QUEUE_GRAPHICS_BIT)
                graphics_index = family_idx;
            if (present_support)
                present_index = family_idx;
            if (graphics_index != UINT32_MAX && present_index != UINT32_MAX)
                break;
        }

        bool has_queues = graphics_index != UINT32_MAX && present_index != UINT32_MAX;
        bool has_extensions = vulkan_backend_check_device_extensions(arena, candidate, device_extensions, array_count(device_extensions));
        bool has_formats = vulkan_backend_check_formats(arena, candidate, vk->surface, image_format, color_space);
        bool has_present_mode = vulkan_backend_check_present_mode(arena, candidate, vk->surface, present_mode);
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
                vk->image_format = image_format;
                vk->color_space = color_space;
                vk->present_mode = present_mode;
                result = true;
            }

        }

    }
    arena_end_scratch(&scratch);
    return result;
}

bool vulkan_backend_create_swapchain(Vulkan_State *vk, uint32_t width, uint32_t height) {
    bool result = true;
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->phys_device, vk->surface, &capabilities);
    vk->extent = capabilities.currentExtent;
    if (vk->extent.width == UINT32_MAX) {
        vk->extent.width = clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        vk->extent.height = clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    uint32_t image_count = clamp(3, capabilities.minImageCount, capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = vk->surface;
    info.minImageCount = image_count;
    info.imageFormat = vk->image_format;
    info.imageColorSpace = vk->color_space;
    info.imageExtent = vk->extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = 0;
    info.preTransform = capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = vk->present_mode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(vk->device, &info, 0, &vk->swapchain) == VK_SUCCESS) {

        uint32_t actual_image_count = 0;
        vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &actual_image_count, 0);
        vk->image_count = actual_image_count;
        vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &actual_image_count, vk->images);

        for (uint32_t image_idx = 0; image_idx < vk->image_count; ++image_idx) {
            VkImageViewCreateInfo image_create_info = {};
            image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            image_create_info.image = vk->images[image_idx];
            image_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            image_create_info.format = vk->image_format;
            image_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            image_create_info.subresourceRange.baseMipLevel = 0;
            image_create_info.subresourceRange.levelCount = 1;
            image_create_info.subresourceRange.baseArrayLayer = 0;
            image_create_info.subresourceRange.layerCount = 1;
            if (vkCreateImageView(vk->device, &image_create_info, 0, &vk->color_image_views[image_idx]) != VK_SUCCESS) {
                result = false;
                print_error("Vulkan Swapchain failed to create color image view\n");
                break;
            }
        }

        print_info("Vulkan Swapchain created successfully.");

    } else {
        print_error("Vulkan failed to create Swapchain.");
        result = false;
    }

    return result;
}

bool vulkan_backend_recreate_swapchain(Vulkan_State *vk, uint32_t width, uint32_t height) {
    bool result = true;
    for (uint32_t idx = 0; idx < vk->image_count; ++idx) {
        vkDestroyFramebuffer(vk->device, vk->framebuffers[idx], 0);
        vkDestroyImageView(vk->device, vk->color_image_views[idx], 0);
    }
    //vkDestroyImage(vk->device, vk->depth_image.image, 0);
    //vkDestroyImageView(vk->device, vk->depth_image.view, 0);
    vkDestroySwapchainKHR(vk->device, vk->swapchain, 0);
    vulkan_backend_create_swapchain(vk, width, height);
    return result;
}

VkRenderPass vulkan_backend_create_render_pass(Vulkan_State *vk, VkFormat color_format) {
    VkAttachmentDescription attachments[] = {
        {
            .format = color_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
    };

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpasses[] = {
        {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment_ref,
        },
    };

    VkSubpassDependency dependencies[] = {
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        },
    };

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = array_count(attachments);
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = array_count(subpasses);
    render_pass_info.pSubpasses = subpasses;
    render_pass_info.dependencyCount = array_count(dependencies);
    render_pass_info.pDependencies = dependencies;

    VkRenderPass result;
    if (vkCreateRenderPass(vk->device, &render_pass_info, 0, &result) != VK_SUCCESS) {
        print_error("Vulkan failed to create render pass");
        return VK_NULL_HANDLE;
    }

    return result;
}

void vulkan_pipeline_info_init(Vulkan_Pipeline_Info *info, VkDevice device) {
    info->device = device;
}

VkShaderModule vulkan_create_shader_module(VkDevice device, uint8_t *bytecode, size_t size) {
    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = size;
    info.pCode = (uint32_t *)bytecode;
    VkShaderModule shader;
    if (vkCreateShaderModule(device, &info, 0, &shader) != VK_SUCCESS) {
        print_error("Failed to create shader module\n");
    }
    return shader;
}

bool vulkan_pipeline_info_add_vertex_shader(Vulkan_Pipeline_Info *info, uint8_t *spv, size_t len) {
    VkShaderModule module = vulkan_create_shader_module(info->device, spv, len);
    if (module == VK_NULL_HANDLE) {
        print_error("Vulkan failed to create vertex shader");
        return false;
    }
    Vulkan_Shader shader = { .type = Shader_Type_Vertex, .handle = module };
    kabarr_append(&info->shaders, shader);
    return true;
}

bool vulkan_pipeline_info_add_fragment_shader(Vulkan_Pipeline_Info *info, uint8_t *spv, size_t len) {
    VkShaderModule module = vulkan_create_shader_module(info->device, spv, len);
    if (module == VK_NULL_HANDLE) {
        print_error("Vulkan failed to create fragment shader");
        return false;
    }
    Vulkan_Shader shader = { .type = Shader_Type_Fragment, .handle = module };
    kabarr_append(&info->shaders, shader);
    return true;
}

void vulkan_pipeline_info_set_render_pass(Vulkan_Pipeline_Info *info, VkRenderPass render_pass) {
    info->render_pass = render_pass;
}

VkShaderStageFlagBits vulkan_shader_get_stage_from_type(Shader_Type type) {
    switch (type) {
        case Shader_Type_Vertex: return VK_SHADER_STAGE_VERTEX_BIT;
        case Shader_Type_Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
        default: abort(); // TODO: add compute shaders
    }
    return 0;
}

bool vulkan_pipeline_create(Vulkan_Pipeline *pipeline, Fixed_Arena *arena, Vulkan_Pipeline_Info *info, VkExtent2D extent) {
    Scratch_Arena scratch = arena_begin_scratch(arena);
    bool result = true;
    if (info->shaders.count > 0) {
        VkPipelineShaderStageCreateInfo *shader_info = push_array(scratch.arena, VkPipelineShaderStageCreateInfo, info->shaders.count);
        for (uint32_t i = 0; i < info->shaders.count; ++i) {
            Vulkan_Shader *shader = &info->shaders.items[i];
            shader_info[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shader_info[i].stage = vulkan_shader_get_stage_from_type(shader->type);
            shader_info[i].module = shader->handle;
            shader_info[i].pName = "main";
        }
        // TODO: Vertex attributes
        VkPipelineVertexInputStateCreateInfo vert_input_info = {};
        vert_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vert_input_info.vertexBindingDescriptionCount = 0;
        vert_input_info.pVertexBindingDescriptions = 0;
        vert_input_info.vertexAttributeDescriptionCount = 0;
        vert_input_info.pVertexAttributeDescriptions = 0;

        VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
        input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_info.primitiveRestartEnable = VK_FALSE;

        if (!(info->flags & VULKAN_PIPELINE_CUSTOM_VIEWPORT)) {
            info->viewport.x = 0.0f;
            info->viewport.y = 0.0f;
            info->viewport.width = (float)extent.width;
            info->viewport.height = (float)extent.height;
            info->viewport.minDepth = 0.0f;
            info->viewport.maxDepth = 1.0f;
        }

        if (!(info->flags & VULKAN_PIPELINE_CUSTOM_SCISSOR)) {
            info->scissor.offset = (VkOffset2D){ 0, 0 };
            info->scissor.extent = extent;
        }

        VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, };
        VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
        dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_info.dynamicStateCount = array_count(dynamic_states);
        dynamic_state_info.pDynamicStates = dynamic_states;

        VkPipelineViewportStateCreateInfo viewport_state_info = {};
        viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state_info.viewportCount = 1;
        viewport_state_info.pViewports = &info->viewport;
        viewport_state_info.scissorCount = 1;
        viewport_state_info.pScissors = &info->scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE; // needs feature. useful for shadow maps.
        rasterizer.rasterizerDiscardEnable = VK_FALSE; // useful for compute-like geometry shaders. process vertices for later use.
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // needs feature. other options VK_POLYGON_MODE_LINE for edges and VK_POLYGON_MODE_POINT for just the vertices
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = info->cull_mode;
        rasterizer.frontFace = info->front_face;
        rasterizer.depthBiasEnable = VK_FALSE; // other depth biasing behaviour available and useful for shadow maps

        // TODO: make this customizeable
        VkPipelineMultisampleStateCreateInfo multisample_info = {};
        multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_info.sampleShadingEnable = VK_FALSE;
        multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample_info.minSampleShading = 1.0f;

        VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {};
        if (info->depth_operation != VK_COMPARE_OP_NEVER) {
            depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depth_stencil_info.depthTestEnable = VK_TRUE;
            depth_stencil_info.depthWriteEnable = VK_TRUE;
            depth_stencil_info.depthCompareOp = info->depth_operation;
            depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
            depth_stencil_info.minDepthBounds = 0.0f;
            depth_stencil_info.maxDepthBounds = 1.0f;
            depth_stencil_info.stencilTestEnable = VK_FALSE;
            depth_stencil_info.front = (VkStencilOpState){};
            depth_stencil_info.back = (VkStencilOpState){};
        }

        VkPipelineColorBlendAttachmentState color_blend_attachment = {};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        if (info->flags & VULKAN_PIPELINE_BLEND_ALPHA) {
            color_blend_attachment.blendEnable = VK_TRUE;
            color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
            color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        } else {
            color_blend_attachment.blendEnable = VK_FALSE;
            color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
            color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }

        VkPipelineColorBlendStateCreateInfo color_blend_info = {};
        color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_info.logicOpEnable = VK_FALSE;
        color_blend_info.logicOp = VK_LOGIC_OP_COPY;
        color_blend_info.attachmentCount = 1;
        color_blend_info.pAttachments = &color_blend_attachment;

        VkPipelineLayoutCreateInfo pipeline_layout_info = {};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        // TODO: descriptor sets
        //pipeline_layout_info.setLayoutCount = 1;
        //pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
        pipeline_layout_info.pushConstantRangeCount = 0;
        pipeline_layout_info.pPushConstantRanges = 0;

        if (vkCreatePipelineLayout(info->device, &pipeline_layout_info, 0, &pipeline->layout) == VK_SUCCESS) {
            VkGraphicsPipelineCreateInfo pipeline_info = {};
            pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipeline_info.stageCount = (uint32_t)info->shaders.count;
            pipeline_info.pStages = shader_info;
            pipeline_info.pVertexInputState = &vert_input_info;
            pipeline_info.pInputAssemblyState = &input_assembly_info;
            pipeline_info.pViewportState = &viewport_state_info;
            pipeline_info.pRasterizationState = &rasterizer;
            pipeline_info.pMultisampleState = &multisample_info;
            pipeline_info.pDepthStencilState = &depth_stencil_info;
            pipeline_info.pColorBlendState = &color_blend_info;
            pipeline_info.pDynamicState = &dynamic_state_info;
            pipeline_info.layout = pipeline->layout;
            pipeline_info.renderPass = info->render_pass;
            pipeline_info.subpass = 0;
            pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
            pipeline_info.basePipelineIndex = -1;
            if (vkCreateGraphicsPipelines(info->device, VK_NULL_HANDLE, 1, &pipeline_info, 0, &pipeline->handle) != VK_SUCCESS) {
                result = false;
                print_error("Failed to create graphics pipeline");
            }
        }

    } else {
        print_error("No shaders added to pipeline info.");
        result = false;
    }
    arena_end_scratch(&scratch);
    return result;
}

void vulkan_pipeline_info_free(Vulkan_Pipeline_Info *info) {
    kabarr_free(&info->shaders);
    kabarr_free(&info->attribs);
}

bool vulkan_backend_init(Vulkan_State *vk, Fixed_Arena *scratch, void *window, int width, int height) {
    if (volkInitialize() != VK_SUCCESS) {
        print_error("Volk failed to initialize.");
        return false;
    }

    const char *layers[] = { "VK_LAYER_KHRONOS_validation" };
    const char *extensions[] = { "VK_KHR_surface", VK_PLATFORM_SURFACE };
    if (!vulkan_backend_create_instance(vk, layers, array_count(layers), extensions, array_count(extensions))) return false;
    volkLoadInstance(vk->instance);

#if 0
    Vulkan_Device_Info device_info = {0};
    vulkan_device_info_init(&device_info);
    vulkan_device_choose_color_format(&device_info, VK_FORMAT_B8G8R8A8_SRGB);
    vulkan_device_choose_color_space(&device_info, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    vulkan_device_choose_present_mode(&device_info, VK_PRESENT_MODE_MAILBOX_KHR);
    vulkan_backend_select_device(vk, &device_info);

    Vulkan_Swapchain_Info swapchain_info = {0};
    vulkan_swapchain_info_init(&swapchain_info);
    vulkan_backend_create_swapchain(vk, &swapchain_info);
#endif

    if (!vulkan_backend_create_surface(vk, window)) return false;
    if (!vulkan_backend_select_device(vk, scratch, VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, VK_PRESENT_MODE_MAILBOX_KHR)) return false;
    if (!vulkan_backend_create_swapchain(vk, width, height)) return false;

    return true;
}

bool vulkan_render_pass_add_attachment(Vulkan_Render_Pass_Info *info, VkFormat format, uint32_t sample_count, Vulkan_Attachment_Type type) {

    VkSampleCountFlagBits sample_flag = 0;
    switch (sample_count) {
        case 1: { sample_flag = VK_SAMPLE_COUNT_1_BIT; } break;
        case 2: { sample_flag = VK_SAMPLE_COUNT_2_BIT; } break;
        case 4: { sample_flag = VK_SAMPLE_COUNT_4_BIT; } break;
        case 8: { sample_flag = VK_SAMPLE_COUNT_8_BIT; } break;
        case 16: { sample_flag = VK_SAMPLE_COUNT_16_BIT; } break;
        case 32: { sample_flag = VK_SAMPLE_COUNT_32_BIT; } break;
        case 64: { sample_flag = VK_SAMPLE_COUNT_64_BIT; } break;
        default: {
            print_error("Invalid multisample count. Must be power of 2 up to 64");
            return false;
        }
    }

    // TODO: make this better
    VkAttachmentDescription attachment = {0};
    attachment.format = format;
    attachment.samples = sample_flag;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    kabarr_append(&info->attachments, attachment);

    return true;
}

bool vulkan_pipeline_create_render_pass(Vulkan_Pipeline_Info *info) {
    bool result = true;
    VkAttachmentDescription attachments[] = {
        {
            .format = VK_FORMAT_B8G8R8A8_SRGB,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
    };

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpasses[] = {
        {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment_ref,
        },
    };

    VkSubpassDependency dependencies[] = {
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        },
    };

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = array_count(attachments);
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = array_count(subpasses);
    render_pass_info.pSubpasses = subpasses;
    render_pass_info.dependencyCount = array_count(dependencies);
    render_pass_info.pDependencies = dependencies;

    if (vkCreateRenderPass(info->device, &render_pass_info, 0, &info->render_pass) != VK_SUCCESS) {
        print_error("Vulkan failed to create render pass");
        result = false;
    }

    return result;
}

bool vulkan_backend_create_final_stage(Vulkan_State *vk, Fixed_Arena *transient_arena) {
    Vulkan_Render_Pass_Info render_pass_info = {0};
    // TODO: think about this more after making more render passes and attachments.
#if 0
    vulkan_render_pass_add_color_attachment(&render_pass_info, VK_FORMAT_B8G8R8A8_SRGB, 1);
    vulkan_render_pass_add_depth_attachment(&render_pass_info, VK_FORMAT_D24_UNORM_S8_UINT, 1);
    vulkan_render_pass_create(&pipeline_info.render_pass, &render_pass_info);
#endif

    Vulkan_Pipeline_Info pipeline_info = {0};
    vulkan_pipeline_info_init(&pipeline_info, vk->device);
    if (!vulkan_pipeline_create_render_pass(&pipeline_info)) return false;
    #include "shaders/final.vert.h"
    if (!vulkan_pipeline_info_add_vertex_shader(&pipeline_info, code_shaders_final_vert_spv, code_shaders_final_vert_spv_len)) return false;
    #include "shaders/final.frag.h"
    if (!vulkan_pipeline_info_add_fragment_shader(&pipeline_info, code_shaders_final_frag_spv, code_shaders_final_frag_spv_len)) return false;
    if (!vulkan_pipeline_create(&vk->final_pipeline, transient_arena, &pipeline_info, vk->extent)) return false;
    vulkan_pipeline_info_free(&pipeline_info);

#if 0
    // extended example
    Vulkan_Pipeline pipeline = {0};
    vulkan_pipeline_init(&pipeline, device);
    vulkan_pipeline_add_vertex_shader(&pipeline, vert_spv, vert_len);
    vulkan_pipeline_add_fragment_shader(&pipeline, frag_spv, frag_len);
    vulkan_pipeline_push_vertx_binding(&pipeline, 0, sizeof(Vertex));
    vulkan_pipeline_push_attribute(&pipeline, 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offset_of(Vertex, position));
    vulkan_pipeline_push_attribute(&pipeline, 0, 1, VK_FORMAT_R32G32B32_SFLOAT, offset_of(Vertex, normal));
    vulkan_pipeline_push_attribute(&pipeline, 0, 2, VK_FORMAT_R32G32_SFLOAT,    offset_of(Vertex, texcoord));
    vulkan_pipeline_set_viewport(&pipeline, extent); // set to max extent by default
    vulkan_pipeline_set_scissor(&pipeline, extent); // set to max extent by default
    vulkan_pipeline_set_depth_test(&pipeline, VK_COMPARE_OP_LESS); // disabled by default, but if you call this its enabled
    vulkan_pipeline_cull_face(&pipeline, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE); // disabled by default, but if you call this its enabled
    vulkan_pipeline_enable_alpha(&pipeline); // false by default, but if you call this its set to true
    if (!vulkan_pipeline_create(&pipeline, render_pass)) return false;
#endif

    return true;
}
