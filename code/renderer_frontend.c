
bool renderer_init(void *memory, size_t size, void *window, int width, int height) {
    assert(sizeof(Renderer_State) < size);
    Renderer_State *renderer = (Renderer_State *)memory;
    size_t remaining_size = size - sizeof(Renderer_State);
    renderer->arena = arena_init((uint8_t *)memory + sizeof(Renderer_State), remaining_size);
    if (!vulkan_backend_init(&renderer->vk, &renderer->arena, window, width, height)) return false;
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