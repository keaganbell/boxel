#define NOB_IMPLEMENTATION
#include "nob.h"

#define PROJ_NAME "boxel"

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    
    if (!nob_mkdir_if_not_exists("build")) return 1;

#if defined(_WIN64)
    Nob_Cmd compile = {0};
    nob_cmd_append(&compile, "cl.exe");
    nob_cmd_append(&compile, "-Zi", "-MT", "-nologo", "-FC", "-FS", "-W3");
    nob_cmd_append(&compile, "-Fobuild\\main.obj");
    nob_cmd_append(&compile, "-Fdbuild\\"PROJ_NAME"_compiler.pdb");
    nob_cmd_append(&compile, "-Febuild\\"PROJ_NAME".exe");
    nob_cmd_append(&compile, "-Icode");
    nob_cmd_append(&compile, "-DVOLK_VULKAN_H_PATH=\"vulkan/vulkan.h\"");
    nob_cmd_append(&compile, "code/main.c");
    nob_cmd_append(&compile, "-link");
    nob_cmd_append(&compile, "-incremental:no");
    nob_cmd_append(&compile, "user32.lib");
    nob_cmd_append(&compile, "-pdb:build\\"PROJ_NAME"_linker.pdb");
    if (!nob_cmd_run(&compile)) return 1;
#elif defined(__linux__)
    Nob_Cmd compile = {0};
    nob_cmd_append(&compile, "gcc");
    nob_cmd_append(&compile, "-Wall", "-Wextra");
    nob_cmd_append(&compile, "-o" "build"PROJ_NAME);
    nob_cmd_append(&compile, "-Icode");
    nob_cmd_append(&compile, "-DVOLK_VULKAN_H_PATH=\"vulkan/vulkan.h\"");
    nob_cmd_append(&compile, "code/main.c");
    if (!nob_cmd_run(&compile)) return 1;
#endif

    return 0;
}