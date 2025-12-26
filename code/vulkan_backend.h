#if !defined(VULKAN_BACKEND_H)
#define VULKAN_BACKEND_H

#if defined (_WIN32)
#define VK_PLATFORM_SURFACE "VK_KHR_win32_surface"
#elif defined (__linux__)
#define VK_PLATFORM_SURFACE "VK_KHR_xcb_surface"
#endif

typedef enum {
    Shader_Type_Vertex,
    Shader_Type_Fragment,
} Shader_Type;

typedef struct {
    Shader_Type type;
    VkShaderModule handle;
} Vulkan_Shader;
typedef struct {
    Vulkan_Shader *items;
    size_t count, capacity;
} Vulkan_Shaders;

typedef struct {
    VkVertexInputAttributeDescription *items;
    size_t count, capacity;
} Vulkan_Vertex_Attribs;

#define VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT 8
typedef struct {
    VkSwapchainKHR          handle;
    VkExtent2D              extent;
    VkFormat                image_format;
    VkColorSpaceKHR         color_space;
    VkPresentModeKHR        present_mode;
    uint32_t                image_count;
    VkImage                 images[VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT];
    VkImageView             color_image_views[VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT];
    VkFramebuffer           framebuffers[VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT];
} Vulkan_Swapchain;

typedef enum {
    Vulkan_Attachment_Type_Unknown,
    Vulkan_Attachment_Type_Input,
    Vulkan_Attachment_Type_Color,
    Vulkan_Attachment_Type_Resolve,
    Vulkan_Attachment_Type_Depth_Stencil,
} Vulkan_Attachment_Type;
typedef struct {
    VkAttachmentReference *items;
    size_t count, capacity;
} Vulkan_Attachment_Refs;
typedef struct {
    VkAttachmentDescription *items;
    size_t count, capacity;
} Vulkan_Attachments;
typedef struct {
    Vulkan_Attachments          attachments;
    Vulkan_Attachment_Refs      attachment_refs;
    VkPipelineStageFlags        source_stage_mask;
    VkAccessFlags               source_access_mask;
    VkPipelineStageFlags        dest_stage_mask;
    VkAccessFlags               dest_access_mask;
} Vulkan_Render_Pass_Info;

#define VULKAN_PIPELINE_CUSTOM_VIEWPORT 0x1
#define VULKAN_PIPELINE_CUSTOM_SCISSOR  0x2
#define VULKAN_PIPELINE_BLEND_ALPHA     0x4
typedef struct {
    uint32_t                flags;
    VkDevice                device;
    VkRenderPass            render_pass;
    Vulkan_Shaders          shaders;
    Vulkan_Vertex_Attribs   attribs;
    VkViewport              viewport;
    VkRect2D                scissor;
    VkCompareOp             depth_operation;
    VkCullModeFlags         cull_mode;
    VkFrontFace             front_face;
} Vulkan_Pipeline_Info;

typedef struct {
    VkPipeline          handle;
    VkPipelineLayout    layout;
} Vulkan_Pipeline;



typedef struct {
    VkInstance                  instance;
    VkSurfaceKHR                surface;
    VkDevice                    device;
    VkPhysicalDevice            phys_device;
    VkQueue                     graphics_queue;
    VkQueue                     present_queue;

////// swapchain  ////////////////////////////////////
    VkSwapchainKHR              swapchain;
    VkExtent2D                  extent;
    VkFormat                    image_format;
    VkColorSpaceKHR             color_space;
    VkPresentModeKHR            present_mode;
#define VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT 8
    uint32_t                    image_count;
    VkImage                     images[VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT];
    VkImageView                 color_image_views[VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT];
    VkFramebuffer               framebuffers[VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT];
//////////////////////////////////////////////////////


#define VULKAN_FRAMES_IN_FLIGHT 3
////// final stage  //////////////////////////////////
    VkRenderPass                final_render_pass;
    Vulkan_Pipeline             final_pipeline;
    VkImage                     final_images[VULKAN_FRAMES_IN_FLIGHT];
    VkImageView                 final_image_views[VULKAN_FRAMES_IN_FLIGHT];
    VkFramebuffer               final_framebuffers[VULKAN_FRAMES_IN_FLIGHT];
//////////////////////////////////////////////////////

} Vulkan_State;

#endif