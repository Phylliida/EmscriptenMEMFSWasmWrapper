
#define NDEBUG // needed to prevent support from using logging
#define NO_EMSCRIPTEN_ERROR // flag added to stop errors in wasmfs

#include <syscalls.cpp> // reset
#include <file_table.cpp> // reset
#include <file.cpp> // reset
#include <paths.cpp> // reset
#include "backends/memory_backend.cpp" // reset
#include <support.cpp> // reset

#include <wasmfs.cpp> // reset, only change is wrapping stuff in ifndef NO_EMSCRIPTEN_ERROR to prevent errors from importing fd_write
// this calls stdin to read from it, todo: add hook
// todo: look into stdout
extern "C" {

// when the file system wants to read from stdin
// should return the number of bytes read (which should be len)
ssize_t read_stdin(uint8_t* buf, size_t len, off_t offset) __attribute__((
    __import_module__("fs_wrapper"),
    __import_name__("read_stdin"),
    __warn_unused_result__
));

// when the file system wants to write to stdout
// should return the number of bytes written (which should be len)
ssize_t write_stdout(const uint8_t* buf, size_t len, off_t offset) __attribute__((
    __import_module__("fs_wrapper"),
    __import_name__("write_stdout"),
    __warn_unused_result__
));

// when the file system wants to write to stderr
// should return the number of bytes written (which should be len)
ssize_t write_stderr(const uint8_t* buf, size_t len, off_t offset) __attribute__((
    __import_module__("fs_wrapper"),
    __import_name__("write_stderr"),
    __warn_unused_result__
));


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

#include <special_files.cpp> // reset, the getnull getstdin needed to replace make shared with a constructor
