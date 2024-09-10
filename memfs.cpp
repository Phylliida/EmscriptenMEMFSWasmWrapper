#define NDEBUG // needed to prevent support from using logging
#define NO_EMSCRIPTEN_ERROR // flag added to stop errors in wasmfs
// found https://github.com/cloudflare/workers-wasi, might b useful
// found https://github.com/wasmerio/kernel-wasm
#include <syscalls.cpp>
#include <file_table.cpp>
#include <file.cpp>
#include <paths.cpp>
#include "backends/memory_backend.cpp"
#include <support.cpp>

#include <wasmfs.cpp> // only change is wrapping stuff in ifndef NO_EMSCRIPTEN_ERROR to prevent errors from importing fd_write (stub does not prevent this)

// Imports required:
// read_stdin, write_stdout, write_stderr
//    these do what you'd expect, stdin/stdout/stderr are typically part of a file system
// emscripten_notify_memory_growth
//    some builtin one from emscripten idk how to get rid of, just make an empty stub
// clock_time_get
//    needed to put time data (last modified, etc.) on the in memory files

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

}

#include <special_files.cpp>

extern "C" {
// Documentation from https://wasix.org/docs/api-reference/

// fd_advise
// NOT SUPPORTED, WILL BE IGNORED
// Advise the system about how a file will be used.
// Description
// The fd_advise() function is used to provide advice to the operating system about the intended usage of a file. This advice can help the system optimize performance or memory usage based on the specified parameters.
// Parameters
// fd: The file descriptor to which the advice applies.
// offset: The offset from which the advice applies.
// len: The length from the offset to which the advice applies.
// advice: The advice to be given to the operating system
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_advise(
    __wasi_fd_t fd,
    __wasi_filesize_t offset,
    __wasi_filesize_t len,
    __wasi_advice_t advice
) {
   return __syscall_fadvise64((int)fd, (off_t)offset, (off_t)len, (int)advice);
}

// fd_allocate
// Allocate extra space for a file descriptor.
// Description
// The fd_allocate function is used to allocate additional space for a file descriptor. It allows extending the size of a file or buffer associated with the file descriptor.
// Parameters
// fd: The file descriptor to allocate space for.
// offset: The offset from the start marking the beginning of the allocation.
// len: The length from the offset marking the end of the allocation.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_allocate(__wasi_fd_t fd, off_t offset, off_t len) {
  return __syscall_fallocate(fd, 0, offset, len);
}

// fd_close
// Close an open file descriptor.
// Description
// The fd_close() function is used to close an open file descriptor. For sockets, this function will flush the data before closing the socket.
// Parameters
// fd: The file descriptor mapping to an open file to close.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_close(__wasi_fd_t fd) {   
   return __wasi_fd_close(fd);
}

// fd_datasync
// Synchronize the file data to disk.
// Description
// The fd_datasync() function is used to synchronize the file data associated with a file descriptor to disk. It ensures that any modified data for the file is written to the underlying storage device.
// Parameters
// fd: The file descriptor to synchronize.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_datasync(__wasi_fd_t fd) {
   return __wasi_fd_sync(fd);
}

// fd_fdstat_get
// Get metadata of a file descriptor.
// Description
// The fd_fdstat_get() function is used to retrieve the metadata of a file descriptor. It provides information about the state of the file descriptor, such as its rights, flags, and file type.
// In POSIX systems, file descriptors are small, non-negative integers used to represent open files, sockets, or other I/O resources. They serve as handles that allow processes to read from or write to these resources. The fd_fdstat_get() function allows applications to retrieve information about a specific file descriptor, gaining insights into its properties and characteristics.
// Parameters
// fd: The file descriptor whose metadata will be accessed.
// buf_ptr: A WebAssembly pointer to a memory location where the metadata will be written.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_fdstat_get(__wasi_fd_t fd, __wasi_fdstat_t* stat) {
   return __wasi_fd_fdstat_get(fd, stat);
}

// fd_fdstat_set_flags
// The fd_fdstat_set_flags() function is used to set the file descriptor flags for a given file descriptor. File descriptor flags modify the behavior and characteristics of the file descriptor, allowing applications to customize its behavior according to specific requirements.
// Description
// In POSIX systems, file descriptors are associated with a set of flags that control various aspects of their behavior. These flags provide additional control over file descriptor operations, such as non-blocking mode, close-on-exec behavior, or file status flags. The fd_fdstat_set_flags() function allows applications to modify these flags for a particular file descriptor, altering its behavior as needed.
// Parameters
// fd: The file descriptor to apply the new flags to.
// flags: The flags to apply to the file descriptor.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_fdstat_set_flags(__wasi_fd_t fd, std::uint16_t flags) {
   // stub for now, ignored
   return __WASI_ERRNO_SUCCESS;
}


// fd_fdstat_set_rights
// Set the rights of a file descriptor. This can only be used to remove rights.
// Description
// The fd_fdstat_set_rights() function is used to set the rights of a file descriptor. It allows modifying the access rights associated with the file descriptor by applying new rights or removing existing rights.
// In POSIX systems, file descriptors are associated with access rights that define the operations that can be performed on the file or resource represented by the descriptor. These access rights control actions such as reading, writing, or seeking within the file. The fd_fdstat_set_rights() function enables applications to modify the access rights associated with a file descriptor, restricting or expanding the available operations.
// Parameters
// fd: The file descriptor to apply the new rights to.
// fs_rights_base: The base rights to apply to the file descriptor.
// fs_rights_inheriting: The inheriting rights to apply to the file descriptor.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_fdstat_set_rights(__wasi_fd_t fd, std::uint64_t fs_rights_base, std::uint64_t fs_rights_inheriting) {
   // stub for now, ignored
   return __WASI_ERRNO_SUCCESS;
}

// fd_filestat_get
// Get the metadata of an open file.
// Description
// The fd_filestat_get() function is used to retrieve the metadata of an open file identified by a file descriptor. It provides information about the file's attributes, such as its size, timestamps, and permissions.
// In POSIX systems, file descriptors are used to perform I/O operations on files. The fd_filestat_get() function allows applications to obtain the metadata of an open file, providing access to important details about the file.
// Parameters
// fd: The file descriptor of the open file whose metadata will be read.
// buf: A WebAssembly pointer to a memory location where the metadata will be written.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_filestat_get(__wasi_fd_t fd, __wasi_fdstat_t* stat) {
   return __wasi_fd_fdstat_get(fd, stat);
}

// fd_filestat_set_size
// Change the size of an open file, zeroing out any new bytes.
// Description
// The fd_filestat_set_size() function is used to modify the size of an open file identified by a file descriptor. It allows adjusting the size of the file and zeroing out any newly allocated bytes.
// In POSIX systems, files have a size attribute that represents the amount of data stored in the file. The fd_filestat_set_size() function enables applications to change the size of an open file. When increasing the file size, any newly allocated bytes are automatically filled with zeros.
// Parameters
// fd: The file descriptor of the open file to adjust.
// st_size: The new size to set for the file.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_filestat_set_size(int fd, off_t size) {
   return (__wasi_errno_t)__syscall_ftruncate64(fd, size);
}

// fd_filestat_set_times
// Set timestamp metadata on a file.
// Description
// The fd_filestat_set_times() function is used to set the timestamp metadata on a file identified by a file descriptor. It allows modifying the last accessed time (st_atim) and last modified time (st_mtim) of the file. The function also provides a bit-vector (fst_flags) to control which times should be set.
// In POSIX systems, files have associated timestamp metadata that stores information about the file's access and modification times. The fd_filestat_set_times() function enables applications to update these timestamps. It allows setting the last accessed and last modified times to specific values or using the current time.
// Parameters
// fd: The file descriptor of the file to set the timestamp metadata.
// st_atim: The last accessed time to set.
// st_mtim: The last modified time to set.
// fst_flags: A bit-vector that controls which times to set.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_filestat_set_times(int fd,
   __wasi_timestamp_t st_atim,
   __wasi_timestamp_t st_mtim,
   __wasi_fstflags_t fst_flags) {
   auto openFile = wasmFS.getFileTable().locked().getEntry(fd);
   if (!openFile) {
      return -EBADF;
   }
   auto locked = openFile->locked().getFile();
   
   return __set_times(locked, st_atim, st_mtim);
}

// fd_pread
// Read from the file at the given offset without updating the file cursor. This acts like a stateless version of Seek + Read.
// Description
// The fd_pread() function is used to read data from a file identified by the provided file descriptor (fd) at a specified offset (offset). Unlike regular reading operations, fd_pread() does not update the file cursor, making it a stateless operation. The function reads data into the provided buffers (iovs) and returns the number of bytes read.
// In POSIX systems, file reading operations typically involve updating the file cursor, which determines the next position from which data will be read. However, fd_pread() allows reading data from a specific offset without modifying the file cursor's position. This can be useful in scenarios where applications need to read data from a file at a specific location without altering the cursor's state.
// Parameters
// fd: The file descriptor of the file to read from.
// iovs: A pointer to an array of __wasi_iovec_t structures describing the buffers where the data will be stored.
// iovs_len: The number of vectors (__wasi_iovec_t) in the iovs array.
// offset: The file cursor indicating the starting position from which data will be read.
// nread: A pointer to store the number of bytes read.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_pread(__wasi_fd_t fd,
                               const __wasi_iovec_t* iovs,
                               size_t iovs_len,
                               __wasi_filesize_t offset,
                               __wasi_size_t* nread) {
   return __wasi_fd_pread(fd, iovs, iovs_len, offset, nread);
}

// fd_prestat_get
// Get metadata about a preopened file descriptor.
// Description
// The fd_prestat_get() function is used to retrieve metadata about a preopened file descriptor (fd). Preopened file descriptors represent files or directories that are provided to a WebAssembly module at startup. This function allows obtaining information about such preopened file descriptors.
// The function takes the file descriptor as input and writes the corresponding metadata into the provided buffer (buf) of type __wasi_prestat. The metadata includes information such as the type of the preopened resource.
// Parameters
// fd: The preopened file descriptor to query.
// buf: A pointer to a Prestat structure where the metadata will be written.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_prestat_get(
    __wasi_fd_t fd,
    __wasi_prestat_t *buf) {
   // stub
   return __WASI_ERRNO_SUCCESS;
}

// fd_prestat_dir_name
// Get the directory name associated with a preopened file descriptor.
// Description
// The fd_prestat_dir_name() function is used to retrieve the directory name associated with a preopened file descriptor (fd). It retrieves the directory name from the file system and writes it into the provided buffer (path) up to the specified length (path_len). The function returns an error code indicating the success or failure of the operation.
// When working with preopened file descriptors, which represent files opened by the WebAssembly module's host environment, it can be useful to obtain information about the directory from which the file was opened. The fd_prestat_dir_name() function provides a convenient way to retrieve the directory name associated with a preopened file descriptor.
// Parameters
// fd: The preopened file descriptor to query.
// path: A pointer to a buffer where the directory name will be written.
// path_len: The maximum length of the buffer.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_prestat_dir_name(
    __wasi_fd_t fd,

    /**
     * A buffer into which to write the preopened directory name.
     */
    uint8_t * path,

    __wasi_size_t path_len) {
   // stub
   return __WASI_ERRNO_SUCCESS;
}


// fd_pwrite
// Write to a file without adjusting its offset.
// Description
// The fd_pwrite() function is used to write data to a file identified by the provided file descriptor (fd) without modifying its offset. It takes an array of __wasi_ciovec_t structures (iovs) describing the buffers from which data will be read, the number of vectors (iovs_len) in the iovs array, the offset indicating the position at which the data will be written, and a pointer to store the number of bytes written. The function writes the data to the file at the specified offset and returns the number of bytes written.
// In POSIX systems, writing data to a file typically involves updating the file cursor, which determines the next position at which data will be written. However, the fd_pwrite() function provides a way to write data to a file without adjusting its offset. This can be useful in scenarios where applications need to write data at a specific location in a file without modifying the file cursor's state.
// Parameters
// fd: The file descriptor of the file to write to.
// iovs: A pointer to an array of __wasi_ciovec_t structures describing the buffers from which data will be read.
// iovs_len: The number of vectors (__wasi_ciovec_t) in the iovs array.
// offset: The offset indicating the position at which the data will be written.
// nwritten: A pointer to store the number of bytes written.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_pwrite(
    __wasi_fd_t fd,
    const __wasi_ciovec_t *iovs,
    size_t iovs_len,
    __wasi_filesize_t offset,
    __wasi_size_t *nwritten) {
      return __wasi_fd_pwrite(fd, iovs, iovs_len, offset, nwritten);
}

// fd_read
// Read data from a file descriptor.
// Description
// The fd_read() function is used to read data from a file identified by the provided file descriptor (fd). It takes an array of __wasi_iovec_t structures (iovs) describing the buffers where the data will be stored, the number of vectors (iovs_len) in the iovs array, and a pointer to store the number of bytes read. The function reads data from the file into the specified buffers and returns the number of bytes read.
// In POSIX systems, reading data from a file typically involves updating the file cursor, which determines the next position from which data will be read. The fd_read() function allows reading data from a file without modifying the file cursor's position. This can be useful in scenarios where applications need to read data from a specific location in a file without altering the cursor's state.
// Parameters
// ctx: A mutable reference to the function environment.
// fd: The file descriptor of the file to read from.
// iovs: A pointer to an array of __wasi_iovec_t structures describing the buffers where the data will be stored.
// iovs_len: The number of vectors (__wasi_iovec_t) in the iovs array.
// nread: A pointer to store the number of bytes read.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_read(__wasi_fd_t fd,
                              const __wasi_iovec_t* iovs,
                              size_t iovs_len,
                              __wasi_size_t* nread) {
  return readAtOffset(OffsetHandling::OpenFileState, fd, iovs, iovs_len, nread);
}

// fd_readdir
// Read data from a directory specified by the file descriptor.
// Description
// The fd_readdir() function reads directory entries from a directory identified by the provided file descriptor (fd). It stores the directory entries in the buffer specified by buf and returns the number of bytes stored in the buffer via the bufused pointer.
// Parameters
// fd: The file descriptor of the directory to read from.
// buf: A pointer to the buffer where directory entries will be stored.
// buf_len: The length of the buffer in bytes.
// cookie: The directory cookie indicating the position to start reading from.
// bufused: A pointer to store the number of bytes stored in the buffer.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_readdir(
    __wasi_fd_t fd,
    uint8_t * buf,
    __wasi_size_t buf_len,
   __wasi_dircookie_t cookie,
    __wasi_size_t *bufused
) {
   // todo: support cookies
   *bufused = __syscall_getdents64((int)fd, (intptr_t)buf, (size_t)buf_len);
   return __WASI_ERRNO_SUCCESS;
}

// fd_renumber
// Atomically copy a file descriptor.
// Description
// The fd_renumber() function atomically copies a file descriptor from one location to another. It ensures that the copying operation is performed atomically and returns an Errno value indicating the success or failure of the operation.
// Parameters
// from: The file descriptor to copy.
// to: The location to copy the file descriptor to.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_renumber(
    __wasi_fd_t fd,
    __wasi_fd_t to
) {
   // todo: not sure if this copies, or just links? pretty sure it copies since there's a seperate symlink but should check
   return __syscall_dup3((int)fd, (int)to, 0);
}


// fd_seek
// Update file descriptor offset.
// Description
// The fd_seek() function updates the offset of a file descriptor. It allows you to adjust the offset by a specified number of bytes relative to a given position.
// Parameters
// fd: The file descriptor to update.
// offset: The number of bytes to adjust the offset by.
// whence: The position that the offset is relative to.
// newoffset: A WebAssembly memory pointer where the new offset will be stored.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_seek(
    __wasi_fd_t fd,
    __wasi_filedelta_t offset,
    __wasi_whence_t whence,
    __wasi_filesize_t *newoffset
) {
   return __wasi_fd_seek(fd, offset, whence, newoffset);
}


// fd_sync
// Synchronize file and metadata to disk.
// Description
// The fd_sync() function synchronizes the file and metadata associated with a file descriptor to disk. This ensures that any changes made to the file and its metadata are persisted and visible to other processes.
// Parameters
// fd: The file descriptor to sync.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_sync(__wasi_fd_t fd) {
   return __wasi_fd_sync(fd);
}

// fd_tell
// Get the offset of the file descriptor.
// Description
// The fd_tell() function retrieves the current offset of the specified file descriptor relative to the start of the file.
// Parameters
// fd: The file descriptor to access.
// offset: A wasm pointer to a Filesize where the offset will be written.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_tell(
    __wasi_fd_t fd,
    __wasi_filesize_t *offset
) {
  auto openFile = wasmFS.getFileTable().locked().getEntry(fd);
  if (!openFile) {
    return __WASI_ERRNO_BADF;
  }

  auto lockedOpenFile = openFile->locked();

  *offset = lockedOpenFile.getPosition();

  if (*offset < 0) {
    return __WASI_ERRNO_INVAL;
  }
  return __WASI_ERRNO_SUCCESS;
}

// fd_write
// Write data to the file descriptor.
// Description
// The fd_write() function writes data from one or more buffers to the specified file descriptor. It is similar to the POSIX write() function, but with additional support for writing multiple non-contiguous buffers in a single function call using the iovs parameter.
// Parameters
// fd: The file descriptor (opened with writing permission) to write to.
// iovs: A wasm pointer to an array of __wasi_ciovec_t structures, each describing a buffer to write data from.
// iovs_len: The length of the iovs array.
// nwritten: A wasm pointer to an M::Offset value where the number of bytes written will be written.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_write(__wasi_fd_t fd,
                               const __wasi_ciovec_t* iovs,
                               size_t iovs_len,
                               __wasi_size_t* nwritten) {
  return writeAtOffset(
    OffsetHandling::OpenFileState, fd, iovs, iovs_len, nwritten);
}

// path_create_directory
// Create a directory at a path.
// Description
// The path_create_directory() function creates a new directory at the specified path relative to the given directory. It requires the PATH_CREATE_DIRECTORY right to be set on the directory where the new directory will be created.
// On POSIX systems, a similar functionality is provided by the mkdir() function. It creates a new directory with the given name and the specified permission mode. The mkdir() function is part of the POSIX standard and is widely supported across different platforms.
// Parameters
// fd: The file descriptor representing the directory that the path is relative to.
// path: A wasm pointer to a null-terminated string containing the path data.
// path_len: The length of the path string.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t path_create_directory(
    __wasi_fd_t fd,
    const char *path,
    size_t path_len) {
      return __syscall_mkdirat((int)dirfd, (intptr_t)path, S_IRUGO | S_IXUGO); 
}

// path_filestat_get
// Access metadata about a file or directory.
// Description
// The path_filestat_get() function allows accessing metadata (file statistics) for a file or directory specified by a path relative to the given directory. It retrieves information such as the size, timestamps, and file type.
// On POSIX systems, a similar functionality is provided by the stat() or lstat() functions, depending on whether symbolic links should be followed or not. These functions retrieve information about a file or symbolic link and store it in a struct stat object.
// Parameters
// fd: The file descriptor representing the directory that the path is relative to.
// flags: Flags to control how the path is understood.
// path: A wasm pointer to a null-terminated string containing the file path.
// path_len: The length of the path string.
// buf: A wasm pointer to a Filestat object where the metadata will be stored.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t path_filestat_get(
    __wasi_fd_t fd,
    __wasi_lookupflags_t flags,
    const char *path,
    size_t path_len,
   __wasi_fdstat_t *buf
) {
   auto parsed = path::parseFile((char*)path);
   if (auto err = parsed.getError()) {
     return err;
   }
   return __get_file_stat(parsed.getFile(), buf);
}

// path_filestat_set_times
// Update time metadata on a file or directory.
// Description
// The path_filestat_set_times() function allows updating the time metadata (last accessed time and last modified time) for a file or directory specified by a path relative to the given directory.
// On POSIX systems, a similar functionality is provided by the utimensat() function. It updates the timestamps (access time and modification time) of a file or directory with nanosecond precision.
// Parameters
// fd: The file descriptor representing the directory that the path is relative to.
// flags: Flags to control how the path is understood.
// path: A wasm pointer to a null-terminated string containing the file path.
// path_len: The length of the path string.
// st_atim: The timestamp that the last accessed time attribute is set to.
// st_mtim: The timestamp that the last modified time attribute is set to.
// fst_flags: A bitmask controlling which attributes are set.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t path_filestat_set_times(
    __wasi_fd_t fd,
    __wasi_lookupflags_t flags,
    const char *path,
    size_t path_len,
    __wasi_timestamp_t st_atim,
    __wasi_timestamp_t st_mtim,
   __wasi_fstflags_t fst_flags
) {
   auto parsed = path::parseFile((char*)path, fd);
   if (auto err = parsed.getError()) {
     return err;
   }   
   return __set_times(parsed.getFile(), st_atim, st_mtim);
}

// path_link
// Create a hard link.
// Description
// The path_link() function creates a hard link between two files. It creates a new directory entry with the specified name in the destination directory, which refers to the same underlying file as the source file.
// On POSIX systems, a similar functionality is provided by the link() function. It creates a new link (directory entry) for an existing file. The new link and the original file refer to the same inode and share the same content.
// Parameters
// old_fd: The file descriptor representing the directory that the old_path is relative to.
// old_flags: Flags to control how the old_path is understood.
// old_path: A wasm pointer to a null-terminated string containing the old file path.
// old_path_len: The length of the old_path string.
// new_fd: The file descriptor representing the directory that the new_path is relative to.
// new_path: A wasm pointer to a null-terminated string containing the new file path.
// new_path_len: The length of the new_path string.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t path_link(
    __wasi_fd_t old_fd,
    __wasi_lookupflags_t old_flags,
    const char *old_path,
    size_t old_path_len,
    __wasi_fd_t new_fd,
    const char *new_path,
   size_t new_path_len
) {
   // todo: it's a lil sus that it doesn't use old_fd, look into that
   return __syscall_symlinkat((intptr_t)new_path, new_fd, (intptr_t)old_path);
}

// path_open
// Open a file located at the given path.
// Description
// The path_open() function opens a file or directory at the specified path relative to the given directory. It provides various options for how the file will be opened, including read and write access, creation flags, and file descriptor flags.
// On POSIX systems, a similar functionality is provided by the open() function. It opens a file or directory with the specified flags and mode. The open() function is a widely used system call for file operations in POSIX-compliant operating systems.
// Parameters
// dirfd: The file descriptor representing the directory that the file is located in.
// dirflags: Flags specifying how the path will be resolved.
// path: A wasm pointer to a null-terminated string containing the path of the file or directory to open.
// path_len: The length of the path string.
// o_flags: Flags specifying how the file will be opened.
// fs_rights_base: The rights of the created file descriptor.
// fs_rights_inheriting: The rights of file descriptors derived from the created file descriptor.
// fs_flags: The flags of the file descriptor.
// fd: A wasm pointer to a WasiFd variable where the new file descriptor will be stored.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t path_open(
    __wasi_fd_t dirfd,
    __wasi_lookupflags_t dirflags,
    const char *path,
    size_t path_len,
    __wasi_oflags_t oflags,
    __wasi_rights_t fs_rights_base,

    __wasi_rights_t fs_rights_inherting,

    __wasi_fdflags_t fdflags,
    __wasi_fd_t *opened_fd) {

   mode_t mode = 0;
   *opened_fd = __syscall_openat2((int)dirfd,
      (intptr_t)path,
      dirflags,
      oflags,
      mode);
   return __WASI_ERRNO_SUCCESS;
}


// path_readlink
// Read the value of a symlink.
// Description
// The path_readlink() function reads the target path that a symlink points to. It requires the PATH_READLINK right to be set on the base directory from which the symlink is understood.
// On POSIX systems, a similar functionality is provided by the readlink() function. It reads the value of a symbolic link and stores it in a buffer. The readlink() function is part of the POSIX standard and is widely supported across different platforms.
// Parameters
// dir_fd: The file descriptor representing the base directory from which the symlink is understood.
// path: A wasm pointer to a null-terminated string containing the path to the symlink.
// path_len: The length of the path string.
// buf: A wasm pointer to a buffer where the target path of the symlink will be written.
// buf_len: The available space in the buffer pointed to by buf.
// buf_used: A wasm pointer to a variable where the number of bytes written to the buffer will be stored.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t path_readlink(
    __wasi_fd_t dir_fd,
   const char *path,
    size_t path_len,
    uint8_t * buf,
    __wasi_size_t buf_len,
    __wasi_size_t *buf_used
) {
   int bytes = __syscall_readlinkat((int)dir_fd,
                         (intptr_t)path,
                         (intptr_t)buf,
                         (size_t)buf_len);
   if (bytes < 0) {
      return -bytes;
   }
   *buf_used = bytes;
   return __WASI_ERRNO_SUCCESS;
}

// path_remove_directory
// Remove a directory.
// Description
// The path_remove_directory() function removes a directory specified by the given path. It requires the PATH_REMOVE_DIRECTORY right to be set on the directory.
// On POSIX systems, a similar functionality is provided by the rmdir() function. It removes an empty directory with the specified path. The rmdir() function is part of the POSIX standard and is widely supported across different platforms.
// Parameters
// fd: The file descriptor representing the base directory from which the path is resolved.
// path: A wasm pointer to a null-terminated string containing the path of the directory to remove.
// path_len: The length of the path string.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t path_remove_directory(
    __wasi_fd_t fd,

    /**
     * The path to a directory to remove.
     */
    const char *path,

    /**
     * The length of the buffer pointed to by `path`.
     */
    size_t path_len
) {
   return __syscall_unlinkat(fd, (intptr_t)path, AT_REMOVEDIR);
}

// path_rename
// Rename a file or directory.
// Description
// The path_rename() function renames a file or directory specified by the given path. It requires the PATH_RENAME_SOURCE right on the source directory and the PATH_RENAME_TARGET right on the target directory.
// On POSIX systems, a similar functionality is provided by the rename() function. It renames a file or directory with the specified source and target paths. The rename() function is part of the POSIX standard and is widely supported across different platforms.
// Parameters
// old_fd: The file descriptor representing the base directory for the source path.
// old_path: A wasm pointer to a null-terminated string containing the source path of the file or directory to be renamed.
// old_path_len: The length of the old_path string.
// new_fd: The file descriptor representing the base directory for the target path.
// new_path: A wasm pointer to a null-terminated string containing the target path with the new name for the file or directory.
// new_path_len: The length of the new_path string.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t path_rename(
    __wasi_fd_t old_fd,
    const char *old_path,
    size_t old_path_len,
    __wasi_fd_t new_fd,
    const char *new_path,
    size_t new_path_len) {
   return __syscall_renameat(old_fd,
                       (intptr_t)old_path,
                       (int)new_fd,
                       (intptr_t)new_path);
}

// path_symlink
// Create a symlink.
// Description
// The path_symlink() function creates a symbolic link (symlink) with the specified source path pointing to the target path. It requires the PATH_SYMLINK right on the base directory.
// On POSIX systems, a similar functionality is provided by the symlink() function. It creates a symbolic link with the specified source and target paths. The symlink() function is part of the POSIX standard and is widely supported across different platforms.
// Parameters
// old_path: A wasm pointer to a null-terminated string containing the source path of the symlink.
// old_path_len: The length of the old_path string.
// fd: The file descriptor representing the base directory from which the paths are understood.
// new_path: A wasm pointer to a null-terminated string containing the target path where the symlink will be created.
// new_path_len: The length of the new_path string.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t path_symlink(
    const char *old_path,
   size_t old_path_len,
    __wasi_fd_t fd,
    const char *new_path,
    size_t new_path_len) {
      return __syscall_symlinkat((intptr_t)new_path, fd, (intptr_t)old_path);
}

// path_unlink_file
// Unlink a file, deleting it if the number of hardlinks is 1.
// Description
// The path_unlink_file() function unlinks a file at the specified path. If the file has only one hardlink (i.e., its link count is 1), it will be deleted from the file system. It requires the PATH_UNLINK_FILE right on the base file descriptor.
// On POSIX systems, a similar functionality is provided by the unlink() function. It removes the specified file from the file system. If the file has no other hardlinks, it is completely deleted. The unlink() function is part of the POSIX standard and is widely supported across different platforms.
// Parameters
// dirfd: The file descriptor representing the base directory from which the path is understood.
// path: A wasm pointer to a null-terminated string containing the path of the file to be unlinked.
// path_len: The length of the path string.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t path_unlink_file(
    __wasi_fd_t dirfd,
    const char *path,
    size_t path_len
) {
   // 0 flags for file
   return __syscall_unlinkat((int)dirfd, (intptr_t)path, 0);
}
}