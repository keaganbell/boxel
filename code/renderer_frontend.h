#if !defined(RENDERER_FRONTEND_H)
#define RENDERER_FRONTEND_H

typedef struct {
    Fixed_Arena arena;
    union {
        Vulkan_State vk;
    };
} Renderer_State;

#endif