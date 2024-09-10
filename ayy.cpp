
#define NDEBUG // needed to prevent support from using logging
#define NO_EMSCRIPTEN_ERROR // flag added to stop errors in wasmfs

#include <syscalls.cpp> // reset
#include <file_table.cpp> // reset
#include <file.cpp> // reset
#include <paths.cpp> // reset
#include "backends/memory_backend.cpp" // reset
#include <support.cpp> // reset

#include <wasmfs.cpp> // reset, only change is wrapping stuff in ifndef NO_EMSCRIPTEN_ERROR
// this calls stdin to read from it, todo: add hook
// todo: look into stdout
#include <special_files.cpp> // reset, the getnull getstdin needed to replace make shared with a constructor
extern "C" {
  
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_write(__wasi_fd_t fd,
                               const __wasi_ciovec_t* iovs,
                               size_t iovs_len,
                               __wasi_size_t* nwritten) {
  return writeAtOffset(
    OffsetHandling::OpenFileState, fd, iovs, iovs_len, nwritten);
}

EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_read(__wasi_fd_t fd,
                              const __wasi_iovec_t* iovs,
                              size_t iovs_len,
                              __wasi_size_t* nread) {
  return readAtOffset(OffsetHandling::OpenFileState, fd, iovs, iovs_len, nread);
}
}