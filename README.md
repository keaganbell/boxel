# Summary
This voxel engine is a personal project just to practice certain programming techniques such as multi-threading "wide by default" and simple forward rendering with vulkan.


## Build
### Windows
Be in an msvc developer environment then just bootstrap nob and run nob.exe.

```cmd
cl -Fenob.exe -nologo nob.c -link -incremental:no
```
```cmd
./nob.exe
```

### Linux
Bootstrap nob with gcc, then run nob.
```cmd
gcc nob.c -o nob
```
```cmd
./nob
```

## Run the Program
An executable called boxel will be in the build folder.

### Windows
```
.\build\boxel.exe
```
### Linux
```
./build/boxel
```
