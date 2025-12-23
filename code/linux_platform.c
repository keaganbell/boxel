#include <unistd.h>
#include <pthread.h>
#include <X11/keysym.h>


void *window_init(char *title, int width, int height) {
    XCB_Window *window = (XCB_Window *)malloc(sizeof(XCB_Window));
    window->connection = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(window->connection))
        return NULL;
    const xcb_setup_t *setup = xcb_get_setup(window->connection);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    xcb_screen_t *screen = it.data;
    xcb_window_t root = screen->root;
    window->handle = xcb_generate_id(window->connection);

    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;

    uint32_t values[] = {
        screen->black_pixel,
        XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_KEY_PRESS |
        XCB_EVENT_MASK_KEY_RELEASE |
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_POINTER_MOTION |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY
    };

    xcb_create_window(
        window->connection,
        XCB_COPY_FROM_PARENT,
        window->handle,
        root,
        100, 100,          // x, y
        width, height,     // width, height
        0,                 // border
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        screen->root_visual,
        mask,
        values
    );

    xcb_change_property(
        window->connection,
        XCB_PROP_MODE_REPLACE,
        window->handle,
        XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING,
        8,
        strlen(title),
        title
    );

    xcb_intern_atom_cookie_t proto_cookie = xcb_intern_atom(window->connection, 0, 12, "WM_PROTOCOLS");
    xcb_intern_atom_reply_t *proto = xcb_intern_atom_reply(window->connection, proto_cookie, NULL);
    xcb_intern_atom_cookie_t wm_del_cookie = xcb_intern_atom(window->connection, 0, 16, "WM_DELETE_WINDOW");
    xcb_intern_atom_reply_t *wm_del = xcb_intern_atom_reply(window->connection, wm_del_cookie, NULL);
    xcb_change_property(
        window->connection,
        XCB_PROP_MODE_REPLACE,
        window->handle,
        proto->atom,
        XCB_ATOM_ATOM,
        32, 1,
        &wm_del->atom
    );

    window->symbols = xcb_key_symbols_alloc(window->connection);

    xcb_map_window(window->connection, window->handle);
    xcb_flush(window->connection);

    return window;
}

Key_Code get_key_code(xcb_key_symbols_t *syms, uint8_t code) {
    xcb_keysym_t sym = xcb_key_symbols_get_keysym(syms, code, 0);
    switch (sym) {
        case XK_space: return Key_Code_Space;
    }
    return Key_Code_Unknown;
}

void process_events(Controller *cont, void *data, bool *window_should_close) {
    XCB_Window *window = (XCB_Window *)data;

    Controller temp = {0};
    memcpy(&temp, cont, sizeof(Controller));
    memset(cont, 0, sizeof(Controller));
    for (int i = 0; i < Key_Code_Count; ++i) {
        cont->keys[i].down = temp.keys[i].down;
    }

    for (;;) {
        xcb_generic_event_t *event = xcb_wait_for_event(window->connection);
        if (!event) break;

        switch (event->response_type & ~0x80) {
            case XCB_EXPOSE: {
                // redraw here
            } break;
            case XCB_KEY_PRESS: {
                xcb_key_press_event_t *e = (void *)event;
                Key_Code code = get_key_code(window->symbols, e->detail);
                update_button(cont, code, false);
                static int counter;
                print_info("Key pressed %d", counter++);
            } break;
            case XCB_KEY_RELEASE: {
                xcb_key_press_event_t *e = (void *)event;
                Key_Code code = get_key_code(window->symbols, e->detail);
                update_button(cont, code, true);
            } break;
            case XCB_BUTTON_PRESS: {
            } break;
            case XCB_BUTTON_RELEASE: {
            } break;
            case XCB_MOTION_NOTIFY: {
            } break;
            case XCB_CONFIGURE_NOTIFY: {
                xcb_configure_notify_event_t *e = (void *)event;
                // resize: e->width, e->height 
            } break;
            case XCB_CLIENT_MESSAGE: {
                // WM close button usually lands here
            } break;
            free(event);
        }
    }
}

int get_max_thread_count() {
    long online = sysconf(_SC_NPROCESSORS_ONLN); 
    long total = sysconf(_SC_NPROCESSORS_CONF);
    print_info("%d out of %d cpus are online", online, total);
    return online;
}

Thread create_thread(pfn_thread_func entry_point, void *params) {
    pthread_t result;
    int rc = pthread_create(&result, NULL, entry_point, params);
    if (rc != 0) return (Thread)0;
    return (Thread)result;
}

void join_thread(Thread thread) {
    pthread_join((pthread_t)thread, NULL);
}

void barrier_create(Barrier *barrier, uint32_t count) {
    barrier->handle = malloc(sizeof(pthread_barrier_t));
    pthread_barrier_init((pthread_barrier_t *)barrier->handle, NULL, count);
}

void barrier_wait(Barrier *barrier) {
    pthread_barrier_wait((pthread_barrier_t *)barrier->handle);
}

void barrier_destroy(Barrier *barrier) {
    pthread_barrier_destroy((pthread_barrier_t *)barrier->handle);
    free(barrier->handle);
    barrier->handle = NULL;
}
