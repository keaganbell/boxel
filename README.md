# Summary

This is a repo that provides a portable template for vulkan using volk and nob on windows with msvc compiler. The only external dependency is vulkan-1.dll which is typically shipped with GPU drivers.

## Build
Be in an msvc developer environment then just bootstrap nob and run ./nob.exe

```cmd
cl -Fenob.exe -nologo nob.c -link -incremental:no
```

```cmd
./nob.exe
```