#define NOB_IMPLEMENTATION
#include "nob.h"

#define PROJ_NAME "boxel"

#if defined(_WIN32)
#define GLSLC_COMMAND "bin\\glslc.exe"
#define XXD_COMMAND "bin\\xxd.exe"
#elif defined (__linux__)
#error NYI
#endif

bool compile_shaders();
bool compile_program();



int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    
    if (!nob_mkdir_if_not_exists("build")) return 1;
    if (!compile_shaders()) return 1;
    if (!compile_program()) return 1;
    return 0;
}



bool compile_shaders() {
    Nob_File_Paths files = {0};
    if (!nob_read_entire_dir("code/shaders", &files)) return false;

    Nob_File_Paths shader_source = {0};
    for (int file_idx = 0; file_idx < files.count; ++file_idx) {
        const char *filename = files.items[file_idx];
        int len = strlen(filename);
        if (len > 5) {
            const char *slice = filename + len - 5;
            if (strcmp(slice, ".frag") == 0 || strcmp(slice, ".vert") == 0)
                nob_da_append(&shader_source, filename);
        }
    }

    for (int shader_idx = 0; shader_idx < shader_source.count; ++shader_idx) {
        const char *shader_filename = shader_source.items[shader_idx];

        Nob_String_Builder input = {0};
        nob_sb_append_cstr(&input, "code/shaders/");
        nob_sb_append_cstr(&input, shader_filename);
        nob_sb_append_null(&input);

        Nob_String_Builder output = {0};
        nob_sb_append_cstr(&output, "code/shaders/");
        nob_sb_append_cstr(&output, shader_filename);
        nob_sb_append_cstr(&output, ".spv");
        nob_sb_append_null(&output);

        Nob_Cmd compile_spv = {0};
        nob_cmd_append(&compile_spv, GLSLC_COMMAND);
        nob_cmd_append(&compile_spv, input.items);
        nob_cmd_append(&compile_spv, "-o", output.items);

        if (!nob_cmd_run(&compile_spv)) return false;

        nob_sb_free(input);
        nob_sb_free(output);
        nob_cmd_free(compile_spv);
    }

    for (int shader_idx = 0; shader_idx < shader_source.count; ++shader_idx) {
        const char *shader_filename = shader_source.items[shader_idx];

        Nob_String_Builder input = {0};
        nob_sb_append_cstr(&input, "code/shaders/");
        nob_sb_append_cstr(&input, shader_filename);
        nob_sb_append_cstr(&input, ".spv");
        nob_sb_append_null(&input);

        Nob_String_Builder output = {0};
        nob_sb_append_cstr(&output, "code/shaders/");
        nob_sb_append_cstr(&output, shader_filename);
        nob_sb_append_cstr(&output, ".h");
        nob_sb_append_null(&output);

        Nob_Cmd convert_header = {0};
        nob_cmd_append(&convert_header, XXD_COMMAND);
        nob_cmd_append(&convert_header, "-i");
        nob_cmd_append(&convert_header, input.items);
        nob_cmd_append(&convert_header, output.items);

        if (!nob_cmd_run(&convert_header)) return false;

        nob_delete_file(input.items);

        nob_sb_free(input);
        nob_sb_free(output);
        nob_cmd_free(convert_header);
    }

    return true;
}


bool compile_program() {
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
    if (!nob_cmd_run(&compile)) return false;
#elif defined(__linux__)
    Nob_Cmd compile = {0};
    nob_cmd_append(&compile, "gcc");
    nob_cmd_append(&compile, "-Wall", "-Wextra");
    nob_cmd_append(&compile, "-Wno-unused-parameter");
    nob_cmd_append(&compile, "-Wno-unused-variable");
    nob_cmd_append(&compile, "-ggdb");
    nob_cmd_append(&compile, "-o" "build/"PROJ_NAME);
    nob_cmd_append(&compile, "-Icode/");
    nob_cmd_append(&compile, "-DVOLK_VULKAN_H_PATH=\"vulkan/vulkan.h\"");
    nob_cmd_append(&compile, "code/main.c");
    nob_cmd_append(&compile, "-lxcb", "-lxcb-keysyms");
    if (!nob_cmd_run(&compile)) return false;
#endif
    return true;
}
