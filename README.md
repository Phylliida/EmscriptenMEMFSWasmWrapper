# EmscriptenMEMFSWasmWrapper
Simple minimal code that results in a wasm file that can act as an in-memory file system, using Emscripten MEMFS

## Exports

Exports [all WASI file commands](https://wasix.org/docs/api-reference) (those starting with `path_` or `fd_`)

## Imports

Only imports (functions you need to define) are:

```c++
// stdin/stdout/stderr are typically part of a file system
size_t read_stdin(uint8_t* buf, size_t len, off_t offset);
size_t write_stdout(const uint8_t* buf, size_t len, off_t offset);
size_t write_stderr(const uint8_t* buf, size_t len, off_t offset);
```

```c++
// some builtin one from emscripten i haven't yet figured out how to get rid of, just make it an empty stub
void emscripten_notify_memory_growth(size_t memory_index);
```

And [clock_time_get](https://github.com/emscripten-core/emscripten/blob/9803070730e1fe8365eb44ac900c1d1751d1c2a6/system/include/wasi/api.h#L1755) which is used to put the timestamps on files.

```c++
uint16_t __wasi_clock_time_get(uint32_t clock_id, uint64_t precision, uint64_t *time);
```


