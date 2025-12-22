#include <windows.h>

static bool global_platform_window_should_close;

LRESULT CALLBACK main_callback(HWND window, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY:
        case WM_QUIT: {
            global_platform_window_should_close = true;
        } break;
        default:
            return DefWindowProc(window, msg, wparam, lparam);
    }
    return 0;
}

void *window_init(char *title, int width, int height) {
    HINSTANCE instance = GetModuleHandle(0);
    WNDCLASS window_class = {0};
    window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = main_callback;
    window_class.hInstance = instance;
    window_class.hIcon = NULL;
    window_class.hCursor = NULL;
    window_class.lpszClassName = "Main Window Class";
    RegisterClassA(&window_class);
    HWND window = CreateWindowA(
        window_class.lpszClassName,
        title,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, instance, NULL
    );
    if (!window) {
        print_error("Windows failed to create window.");
        return NULL;
    }
    return window;
}

void process_events(Controller *cont, void *window, bool *window_should_close) {
    HWND handle = (HANDLE)window;
    *window_should_close = global_platform_window_should_close;

    Controller temp = {0};
    memcpy(&temp, cont, sizeof(Controller));
    memset(cont, 0, sizeof(Controller));
    for (int i = 0; i < Key_Code_Count; ++i) {
        cont->keys[i].down = temp.keys[i].down;
    }

    MSG msg;
    while (PeekMessageA(&msg, handle, 0, 0, PM_REMOVE)) {
        switch (msg.message) {

            case WM_KEYDOWN:
            case WM_KEYUP: {
                bool was_down = ((msg.lParam & (1 << 30)) == 0);
                bool is_up    = ((msg.lParam & (1 << 31)) != 0);
                if (was_down != is_up) {
                    if (msg.wParam == VK_SPACE) {
                        update_button(cont, Key_Code_Space, is_up);
                    } else if (msg.wParam == VK_ESCAPE) {
                        update_button(cont, Key_Code_Escape, is_up);
                    } else if (msg.wParam == VK_SHIFT) {
                        update_button(cont, Key_Code_Shift, is_up);
                    }
                }
            } break;

            case WM_LBUTTONDOWN: {
                update_button(cont, Key_Code_Mouse_Left, false);
            } break;
            case WM_LBUTTONUP: {
                update_button(cont, Key_Code_Mouse_Left, true);
            } break;
            case WM_RBUTTONDOWN: {
                update_button(cont, Key_Code_Mouse_Right, false);
            } break;
            case WM_RBUTTONUP: {
                update_button(cont, Key_Code_Mouse_Right, true);
            } break;

            default: {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        }
    }
}

int get_max_thread_count() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors;
}

Thread create_thread(pfn_thread_func entry_point, void *params) {
    HANDLE thread = CreateThread(
        NULL,           // default security
        0,              // default stack size
        entry_point,    // thread function
        params,         // parameter
        0,              // default creation flags
        NULL            // thread ID (optional)
    );
    if (!thread) print_error("Windows failed to create thread.");
    return (Thread)thread;
}

void join_thread(Thread thread) {
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
}

void barrier_create(Barrier *barrier, uint32_t count) {
    barrier->handle = malloc(sizeof(SYNCHRONIZATION_BARRIER));
    InitializeSynchronizationBarrier((SYNCHRONIZATION_BARRIER *)barrier->handle, count, -1);
}
void barrier_wait(Barrier *barrier) {
    EnterSynchronizationBarrier((SYNCHRONIZATION_BARRIER*)barrier->handle, SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY);
}
void barrier_destroy(Barrier *barrier) {
    DeleteSynchronizationBarrier((SYNCHRONIZATION_BARRIER*)barrier->handle);
    free(barrier->handle);
}
