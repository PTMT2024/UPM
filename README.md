## Instruction (TCP Server)
### Initialization
Remove all old CMakefiles.

```
rm CMakeCache.txt
rm -rf CMakeFiles/
```

Run CMake to generate the Makefile.

`cmake .`

Run make clean to remove any previous build artifacts.

`make clean`

Run make to build the project.

`make`

### Rebuild the project

```
cmake .  
make clean
make
```

### Binary
start server
```
./user_space_page_migration
```

reset
```
tcp_client/client enable -p {bench_pid} -h 2
```

update
```
tcp_client/client update -h 2 -m 1
```