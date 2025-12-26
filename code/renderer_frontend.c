
// cpu stages: ?
//  - generate terrain meshes
//  - update particles/emitters
//  - push entity meshes 
//  - fetch models and textures

// gpu stages:
//  - shadows mega texture
//  - depth prepass
//  - opaque forward render
//  - compute passes: particles, raymarching


bool renderer_init(void *memory, size_t size, void *window, int width, int height) {
    assert(sizeof(Renderer_State) < size);
    Renderer_State *renderer = (Renderer_State *)memory;
    size_t remaining_size = size - sizeof(Renderer_State);
    renderer->transient_arena = arena_init((uint8_t *)memory + sizeof(Renderer_State), remaining_size);
    if (!vulkan_backend_init(&renderer->vk, &renderer->transient_arena, window, width, height)) return false;
    if (!vulkan_backend_create_final_stage(&renderer->vk, &renderer->transient_arena)) return false;
    return true;
}

#if 0

void draw_circle();
void draw_line();
void draw_rectangle();
void draw_mesh();

Texture_Handle create_texture();

Buffer_Handle create_mesh();

#endif