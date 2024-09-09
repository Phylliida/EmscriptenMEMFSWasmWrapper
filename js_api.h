// Copyright 2022 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#include <dirent.h>
#include <syscall_arch.h>
#include <unistd.h>

#include "emscripten/system/lib/wasmfs/backend.h"
#include "emscripten/system/lib/wasmfs/file.h"
#include "emscripten/system/lib/wasmfs/paths.h"
using namespace wasmfs;


// TODO: Replace forward declarations with #include <emscripten/wasmfs.h> and
// resolve wasmfs::backend_t namespace conflicts.
__wasi_fd_t wasmfs_create_file(char* pathname, mode_t mode, backend_t backend);
int wasmfs_create_directory(char* path, int mode, backend_t backend);
int wasmfs_unmount(intptr_t path);

// Copy the file specified by the pathname into JS.
// Return a pointer to the JS buffer in HEAPU8.
// The buffer will also contain the file length.
// TODO: Use WasmFS ErrnoError handling instead of aborting on failure.
void* _wasmfs_read_file(char* path);

// Writes to a file, possibly creating it, and returns the number of bytes
// written successfully. If the file already exists, appends to it.
int _wasmfs_write_file(char* pathname, char* data, size_t data_size);

int _wasmfs_mkdir(char* path, int mode);

int _wasmfs_rmdir(char* path);

int _wasmfs_open(char* path, int flags, mode_t mode);

int _wasmfs_allocate(int fd, off_t offset, off_t len);

int _wasmfs_mknod(char* path, mode_t mode, dev_t dev);

int _wasmfs_unlink(char* path);

int _wasmfs_chdir(char* path);

int _wasmfs_symlink(char* old_path, char* new_path);

intptr_t _wasmfs_readlink(char* path);

int _wasmfs_write(int fd, void* buf, size_t count);

int _wasmfs_pwrite(int fd, void* buf, size_t count, off_t offset);

int _wasmfs_chmod(char* path, mode_t mode);

int _wasmfs_fchmod(int fd, mode_t mode);

int _wasmfs_lchmod(char* path, mode_t mode);

int _wasmfs_llseek(int fd, off_t offset, int whence);

int _wasmfs_rename(char* oldpath, char* newpath);

int _wasmfs_read(int fd, void* buf, size_t count);

int _wasmfs_pread(int fd, void* buf, size_t count, off_t offset);

int _wasmfs_truncate(char* path, off_t length);

int _wasmfs_ftruncate(int fd, off_t length);

int _wasmfs_close(int fd);

int _wasmfs_mmap(size_t length, int prot, int flags, int fd, off_t offset);

int _wasmfs_msync(void* addr, size_t length, int flags);

int _wasmfs_munmap(void* addr, size_t length);

int _wasmfs_utime(char* path, long atime_ms, long mtime_ms);

int _wasmfs_stat(char* path, struct stat* statBuf);

int _wasmfs_lstat(char* path, struct stat* statBuf);

// The legacy JS API requires a mountpoint to already exist, so  WasmFS will
// attempt to remove the target directory if it exists before replacing it with
// a mounted directory.
int _wasmfs_mount(char* path, wasmfs::backend_t created_backend);

// WasmFS will always remove the mounted directory, regardless of if the
// directory existed before.
int _wasmfs_unmount(char* path);

// Helper method that identifies what a path is:
//   ENOENT - if nothing exists there
//   EISDIR - if it is a directory
//   EEXIST - if it is a normal file
int _wasmfs_identify(char* path);

struct wasmfs_readdir_state {
  int i;
  int nentries;
  struct dirent** entries;
};

struct wasmfs_readdir_state* _wasmfs_readdir_start(char* path);

const char* _wasmfs_readdir_get(struct wasmfs_readdir_state* state);

void _wasmfs_readdir_finish(struct wasmfs_readdir_state* state);

char* _wasmfs_get_cwd(void);

