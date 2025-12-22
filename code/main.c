
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include "volk/volk.h"
#include "volk/volk.c"

#include "core.h"
#include "vulkan_backend.h"
#include "renderer_frontend.h"

#include "vulkan_backend.c"
#include "renderer_frontend.c"

#if defined(_WIN32)
#include "windows_platform.c"
#endif

static void *window = NULL;
static int window_width = 800;
static int window_height = 600;
static Controller controller;
static bool running = true;

static Thread_Context *threads;
static uint32_t thread_count;
static Barrier barrier;

THREAD_RETURN_TYPE thread_entry_point(void *data) {
    Thread_Context *ctx = (Thread_Context *)data;
    uint32_t thread_index = ctx->index;

    size_t renderer_size = (1ull<<30);
    static void *renderer = NULL;
    if (thread_index == 0) {
        window = window_init("Boxel", window_width, window_height);
        renderer = virtual_alloc(renderer_size);
        if (!renderer_init(renderer, renderer_size, window, window_width, window_height)) {
            print_error("Renderer failed to initialize!");
            running = false;
        } else {
            print_info("thread %d: Vulkan initialized successfully!", thread_index);
        }
    }

    barrier_wait(&barrier);
    while (running) {
        if (thread_index == 0) {
            bool window_should_close;
            process_events(&controller, window, &window_should_close);
            running = !window_should_close;
        }

        barrier_wait(&barrier);
        if (button_pressed(&controller, Key_Code_Mouse_Left)) {
            print_info("thread %d: left mouse pressed.", thread_index);
        }
    }
    return 0;
}

int main(void) {

    thread_count = get_max_thread_count();
    threads = malloc(sizeof(Thread)*thread_count);
    memset(threads, 0, sizeof(Thread)*thread_count);

    barrier_create(&barrier, thread_count);

    for (uint32_t thread_idx = 0; thread_idx < thread_count; ++thread_idx) {
        threads[thread_idx] = (Thread_Context){0};
        threads[thread_idx].handle = create_thread(thread_entry_point, &threads[thread_idx]);
        threads[thread_idx].index = thread_idx;
    }

    for (uint64_t thread_idx = 0; thread_idx < thread_count; ++thread_idx) {
        join_thread(threads[thread_idx].handle);
    }
    return 0;
}
