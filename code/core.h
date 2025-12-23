#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>

#define func

typedef enum {
    Log_Level_Info,
    Log_Level_Error,
} Log_Level;

#define print_error(str, ...) print_log(Log_Level_Error, str, __FILE__, __LINE__, ##__VA_ARGS__)
#define print_info(str, ...) print_log(Log_Level_Info, str, __FILE__, __LINE__, ##__VA_ARGS__)
static inline void print_log(Log_Level level, char *fmt, char *filename, int line_number, ...) {
    char buf[1<<12];
    va_list args;
    va_start(args, line_number);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    switch (level) {
        case Log_Level_Info: {
            fprintf(stdout, "Info (%s:%d): %s\n", filename, line_number, buf);
        } break;
        case Log_Level_Error: {
            fprintf(stdout, "ERROR (%s:%d): %s\n", filename, line_number, buf);
        } break;
    }
}

#if defined(_WIN32)
#include <windows.h>
#define virtual_alloc(x) VirtualAlloc(0, x, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE)
#elif defined(__linux__)
#include <sys/mman.h>
#define virtual_alloc(x) mmap(0, x, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
#endif

#define array_count(x) ((sizeof(x)/sizeof(*(x))))

#define kabarr_max(a, b) ((a) > (b) ? (a) : (b))

#define KABARR_DEFAULT_CAPACITY 64
#define kabarr_grow(arr, expected_capacity)                                         \
    do {                                                                            \
        if (!(arr)->items) {                                                        \
            size_t count = kabarr_max(expected_capacity, KABARR_DEFAULT_CAPACITY);  \
            (arr)->items = malloc(count*sizeof(*(arr)->items));                     \
            assert((arr)->items);                                                   \
            (arr)->capacity = count;                                                \
        }                                                                           \
        if (expected_capacity > (arr)->capacity) {                                  \
            size_t new_capacity = kabarr_max(expected_capacity, 2*(arr)->capacity); \
            void *temp = realloc((arr)->items, new_capacity*sizeof(*(arr)->items)); \
            assert(temp);                                                           \
            (arr)->items = temp;                                                    \
            (arr)->capacity = new_capacity;                                         \
        }                                                                           \
    } while(0)

#define kabarr_append(arr, item) kabarr_append_many(arr, &(item), 1)

#define kabarr_append_many(arr, new_items, item_count)                      \
    do {                                                                    \
        kabarr_grow(arr, (arr)->count + item_count);                        \
        size_t items_size = (item_count)*sizeof(*(arr)->items);             \
        memcpy((arr)->items + (arr)->count, new_items, items_size);         \
        (arr)->count += item_count;                                         \
    } while(0)

#define kabarr_remove_unordered(arr, index)                                                         \
    do {                                                                                            \
        if ((index) >= 0 && (index) < (arr)->count) {                                               \
            memcpy((arr)->items + index, (arr)->items + (arr)->count - 1, sizeof(*(arr)->items))    \
            --(arr)->count;                                                                         \
        }                                                                                           \
    } while(0)

#define kabarr_free(arr)                    \
    do {                                    \
        if ((arr)->items) {                 \
            free((arr)->items);             \
            memset(arr, 0, sizeof(*(arr))); \
        }                                   \
    } while(0)

typedef struct {
    void *base;
    size_t used, capacity;
} Fixed_Arena;

typedef struct {
    Fixed_Arena *arena;
    size_t old_used;
} Scratch_Arena;

#define arena_alloc(a, size)            \
    do {                                \
        void *base = malloc(size);      \
        memset(base, 0, size);          \
        *(a) = arena_init(base, size);  \
    } while(0)

#define arena_free(a)       \
    do {                    \
        free((a)->base);    \
        (a)->base = 0;      \
    } while(0)

static inline Fixed_Arena arena_init(void *base, size_t size) {
    Fixed_Arena result = {0};
    result.base = base;
    result.capacity = size;
    return result;
}

#define push_struct(a, t) (t *)arena_push(a, sizeof(t))
#define push_array(a, t, c) (t *)arena_push(a, (c)*sizeof(t))
static inline void *arena_push(Fixed_Arena *arena, size_t size) {
    void *result = 0;
    if (arena->base && arena->used + size < arena->capacity) {
        result = (uint8_t *)arena->base + arena->used;
        arena->used += size;
        memset(result, 0, size);
    }
    return result;
}

#define arena_reset(a) (a)->used = 0

static inline Scratch_Arena arena_begin_scratch(Fixed_Arena *arena) {
    Scratch_Arena result = {0};
    result.arena = arena;
    result.old_used = arena->used;
    return result;
}

static inline void arena_end_scratch(Scratch_Arena *scratch) {
    scratch->arena->used = scratch->old_used;
    *scratch = (Scratch_Arena){0};
}

int get_max_thread_count();

typedef void *Thread;

typedef struct {
    Thread handle;
    uint32_t index;
} Thread_Context;

typedef struct {
    void *handle;
} Barrier;

#if defined(_WIN32)
#define THREAD_RETURN_TYPE int
#elif defined(__linux__)
#define THREAD_RETURN_TYPE void *
#endif
typedef THREAD_RETURN_TYPE (*pfn_thread_func)(void *params);
Thread create_thread(pfn_thread_func entry_point, void *params);
void join_thread(Thread thread);

void barrier_create(Barrier *barrier, uint32_t count);
void barrier_wait(Barrier *barrier);
void barrier_destroy(Barrier *barrier);

typedef enum {
    Key_Code_Unknown,

    Key_Code_Space,
    Key_Code_Escape,
    Key_Code_Shift,

    Key_Code_W,
    Key_Code_A,
    Key_Code_S,
    Key_Code_D,

    Key_Code_Left,
    Key_Code_Right,
    Key_Code_Up,
    Key_Code_Down,

    Key_Code_F1,
    Key_Code_F2,
    Key_Code_F3,
    Key_Code_F4,
    Key_Code_F5,
    Key_Code_F6,
    Key_Code_F7,
    Key_Code_F8,

    Key_Code_Mouse_Left,
    Key_Code_Mouse_Right,

    Key_Code_Count,
} Key_Code;

typedef union {
    struct {
        bool pressed;
        bool released;
        bool down;
    };
    struct {
        int16_t axis_x;
        int16_t axis_y;
    };
} Key_State;

// Clears with sizeof() so can't use dynamically
// allocated memory in this struct.
typedef struct {
    Key_State keys[Key_Code_Count];
} Controller;

static inline bool button_pressed(Controller *cont, Key_Code code) {
    return cont->keys[code].pressed;
}
static inline bool button_down(Controller *cont, Key_Code code) {
    return cont->keys[code].down;
}
static inline bool button_released(Controller *cont, Key_Code code) {
    return cont->keys[code].released;
}
static inline void update_button(Controller*cont, Key_Code code, bool is_up) {
    cont->keys[code].pressed = !is_up;
    cont->keys[code].down = !is_up;
    cont->keys[code].released = is_up;
}

#if defined(__linux__)
#include <xcb/xcb_keysyms.h>
typedef struct {
    xcb_connection_t *connection;
    xcb_window_t handle;
    xcb_key_symbols_t *symbols;
} XCB_Window;
#endif
