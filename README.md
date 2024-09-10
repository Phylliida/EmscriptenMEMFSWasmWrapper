# EmscriptenWASMFSWrapper

Simple minimal code that results in a [single wasm file](https://github.com/Phylliida/EmscriptenMEMFSWasmWrapper/blob/main/memfs.wasm) that can act as an ephemeral in-memory file system, using Emscripten's WASMFS.

## Exports

Exports [all WASI file commands](https://wasix.org/docs/api-reference) (those starting with `path_` or `fd_`).

Some of those methods are stubs and currently do nothing, and some are missing some functionality, as WASM_FS is still in development. However it has enough functionaity to fill most use cases.

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

## Explanation

Many wasm files request access to the file system, but sometimes it's not safe to do so. This provides a safe alternative, with a file system implemented entirely in wasm, that exists in memory.

This is using emscripten's wasmfs, which is still in development but is generally ready for most use cases. However, wasmfs doesn't have a nice wrapper that makes it wasi-compatible, which is why I made this library.

### What's modified from emscripten?

`syscalls.cpp` has a few extra minor features I added to support some wasi commands.

`wasmfs.cpp` has a new flag
```c++
#define NO_EMSCRIPTEN_ERROR
```
if it is defined, `emscripten_err` will never be called. This prevents `fd_write` from being imported to do error handling.

`special_files.cpp` is modified to call the `stdin/stdout/stderr` imports instead of generating javascript bindings and/or importing `fd_write` and `fd_read`.

Otherwise `wasmfs` is unmodified from emscripten, so as `wasmfs` develops further those features can be easily integrated into here.

## Alternatives

[https://github.com/bytecodealliance/WASI-Virt](https://github.com/bytecodealliance/WASI-Virt) is good, but read-only.

[https://github.com/cloudflare/workers-wasi](https://github.com/cloudflare/workers-wasi) looks promising, I learned about it when I was almost done developing this. It uses [https://github.com/littlefs-project](https://github.com/littlefs-project). The major feature diffrence from here is that `workers-wasi` is missing symlinks, but that's not usually needed.