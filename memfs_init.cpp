// need to do this:https://github.com/emscripten-core/emscripten/issues/22249#issuecomment-2240873930

#include <emscripten.h>
#include <stdio.h>
#include <string.h>

#include <cstdint>
#include <cstddef>

#include <dirent.h>
#include <emscripten/emscripten.h>
#include <emscripten/heap.h>
#include <emscripten/html5.h>
#include <errno.h>
#include <mutex>
#include <poll.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <syscall_arch.h>
#include <unistd.h>
#include <utility>
#include <vector>
#include <wasi/api.h>

#define NDEBUG // prevents boost assert from throwing exceptions and resulting in imports

// just modify cache/sysroot/include/wasi/api.h functions you don't want to have imports to be dummy things instead
// actually stub out
// ../../../system/lib/wasmfs/syscalls.cpp
// and we will define it here (after doing that, delete cache)

//#include "js_api.h"
// normally never include cpp but we gotta do this so symbols aren't multiply defined
// from hack that removes fd_write
//#include "wasmfs/emscripten.cpp"
#include <unistd.h> // getentropy
#include <fcntl.h>
#include <assert.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <map>
#include <sys/stat.h>

#include <boost/unordered_map.hpp>

#include "wasmfs/file.cpp"
#include "wasmfs/special_files.cpp"
#include "wasmfs/file_table.cpp"


#include "wasmfs/backends/memory_backend.cpp"


#include "wasmfs/paths.cpp"
//#include "wasmfs/support.cpp"
//#include "wasmfs/syscalls.cpp"
#include "wasmfs/wasmfs.cpp"
// this lets us remove imports for reasons I don't understand
#include <boost/unordered_map.hpp>


extern "C" {

/*
EMSCRIPTEN_KEEPALIVE
void bees() {
   FILE* file = fopen("bees", "w");
   const char* text = "hello";
   fwrite(text, sizeof(char), strlen(text), file);
   fclose(file);
}
*/

#define js_index_t uintptr_t

// stub out jsimpl since we are not using that, this prevents these imports
void _wasmfs_jsimpl_alloc_file(js_index_t backend, js_index_t index)
{
}
void _wasmfs_jsimpl_free_file(js_index_t backend, js_index_t index)
{

}
__wasi_errno_t _wasmfs_jsimpl_write(js_index_t backend,
                         js_index_t index,
                         const uint8_t* buffer,
                         size_t length,
                         off_t offset)
{
   return 0;
}
__wasi_errno_t _wasmfs_jsimpl_read(js_index_t backend,
                        js_index_t index,
                        const uint8_t* buffer,
                        size_t length,
                        off_t offset)
{
   return 0;
}
__wasi_errno_t _wasmfs_jsimpl_get_size(js_index_t backend, js_index_t index)
{
   return 0;
}
__wasi_errno_t _wasmfs_jsimpl_set_size(js_index_t backend, js_index_t index, off_t size)
{
   return 0;
}

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
   //__syscall_fadvise64((int)fd, (off_t)offset, (off_t)len, (int)advice);
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
  //return __syscall_fallocate(fd, 0, offset, len);
}

// fd_close
// Close an open file descriptor.
// Description
// The fd_close() function is used to close an open file descriptor. For sockets, this function will flush the data before closing the socket.
// Parameters
// fd: The file descriptor mapping to an open file to close.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_close(__wasi_fd_t fd) {
   // Don't bother with this, they are in memory
   return 0;
}

// fd_datasync
// Synchronize the file data to disk.
// Description
// The fd_datasync() function is used to synchronize the file data associated with a file descriptor to disk. It ensures that any modified data for the file is written to the underlying storage device.
// Parameters
// fd: The file descriptor to synchronize.
EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_datasync(__wasi_fd_t fd) {
   //return __syscall_fdatasync(fd);
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
   //return __wasi_fd_fdstat_get(fd, stat);
}

// fd_fdstat_set_flags
// The fd_fdstat_set_flags() function is used to set the file descriptor flags for a given file descriptor. File descriptor flags modify the behavior and characteristics of the file descriptor, allowing applications to customize its behavior according to specific requirements.
// Description
// In POSIX systems, file descriptors are associated with a set of flags that control various aspects of their behavior. These flags provide additional control over file descriptor operations, such as non-blocking mode, close-on-exec behavior, or file status flags. The fd_fdstat_set_flags() function allows applications to modify these flags for a particular file descriptor, altering its behavior as needed.
// Parameters
// fd: The file descriptor to apply the new flags to.
// flags: The flags to apply to the file descriptor.

// fd_fdstat_set_rights
// Set the rights of a file descriptor. This can only be used to remove rights.
// Description
// The fd_fdstat_set_rights() function is used to set the rights of a file descriptor. It allows modifying the access rights associated with the file descriptor by applying new rights or removing existing rights.
// In POSIX systems, file descriptors are associated with access rights that define the operations that can be performed on the file or resource represented by the descriptor. These access rights control actions such as reading, writing, or seeking within the file. The fd_fdstat_set_rights() function enables applications to modify the access rights associated with a file descriptor, restricting or expanding the available operations.
// Parameters
// fd: The file descriptor to apply the new rights to.
// fs_rights_base: The base rights to apply to the file descriptor.
// fs_rights_inheriting: The inheriting rights to apply to the file descriptor.


// fd_filestat_get
// Get the metadata of an open file.
// Description
// The fd_filestat_get() function is used to retrieve the metadata of an open file identified by a file descriptor. It provides information about the file's attributes, such as its size, timestamps, and permissions.
// In POSIX systems, file descriptors are used to perform I/O operations on files. The fd_filestat_get() function allows applications to obtain the metadata of an open file, providing access to important details about the file.
// Parameters
// fd: The file descriptor of the open file whose metadata will be read.
// buf: A WebAssembly pointer to a memory location where the metadata will be written.


// fd_filestat_set_size
// Change the size of an open file, zeroing out any new bytes.
// Description
// The fd_filestat_set_size() function is used to modify the size of an open file identified by a file descriptor. It allows adjusting the size of the file and zeroing out any newly allocated bytes.
// In POSIX systems, files have a size attribute that represents the amount of data stored in the file. The fd_filestat_set_size() function enables applications to change the size of an open file. When increasing the file size, any newly allocated bytes are automatically filled with zeros.
// Parameters
// fd: The file descriptor of the open file to adjust.
// st_size: The new size to set for the file.


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


// fd_prestat_get
// Get metadata about a preopened file descriptor.
// Description
// The fd_prestat_get() function is used to retrieve metadata about a preopened file descriptor (fd). Preopened file descriptors represent files or directories that are provided to a WebAssembly module at startup. This function allows obtaining information about such preopened file descriptors.
// The function takes the file descriptor as input and writes the corresponding metadata into the provided buffer (buf) of type __wasi_prestat. The metadata includes information such as the type of the preopened resource.
// Parameters
// fd: The preopened file descriptor to query.
// buf: A pointer to a Prestat structure where the metadata will be written.


// fd_prestat_dir_name
// Get the directory name associated with a preopened file descriptor.
// Description
// The fd_prestat_dir_name() function is used to retrieve the directory name associated with a preopened file descriptor (fd). It retrieves the directory name from the file system and writes it into the provided buffer (path) up to the specified length (path_len). The function returns an error code indicating the success or failure of the operation.
// When working with preopened file descriptors, which represent files opened by the WebAssembly module's host environment, it can be useful to obtain information about the directory from which the file was opened. The fd_prestat_dir_name() function provides a convenient way to retrieve the directory name associated with a preopened file descriptor.
// Parameters
// fd: The preopened file descriptor to query.
// path: A pointer to a buffer where the directory name will be written.
// path_len: The maximum length of the buffer.


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


// fd_renumber
// Atomically copy a file descriptor.
// Description
// The fd_renumber() function atomically copies a file descriptor from one location to another. It ensures that the copying operation is performed atomically and returns an Errno value indicating the success or failure of the operation.
// Parameters
// from: The file descriptor to copy.
// to: The location to copy the file descriptor to.


// fd_seek
// Update file descriptor offset.
// Description
// The fd_seek() function updates the offset of a file descriptor. It allows you to adjust the offset by a specified number of bytes relative to a given position.
// Parameters
// fd: The file descriptor to update.
// offset: The number of bytes to adjust the offset by.
// whence: The position that the offset is relative to.
// newoffset: A WebAssembly memory pointer where the new offset will be stored.


// fd_sync
// Synchronize file and metadata to disk.
// Description
// The fd_sync() function synchronizes the file and metadata associated with a file descriptor to disk. This ensures that any changes made to the file and its metadata are persisted and visible to other processes.
// Parameters
// fd: The file descriptor to sync.


// fd_tell
// Get the offset of the file descriptor.
// Description
// The fd_tell() function retrieves the current offset of the specified file descriptor relative to the start of the file.
// Parameters
// fd: The file descriptor to access.
// offset: A wasm pointer to a Filesize where the offset will be written.


// fd_write
// Write data to the file descriptor.
// Description
// The fd_write() function writes data from one or more buffers to the specified file descriptor. It is similar to the POSIX write() function, but with additional support for writing multiple non-contiguous buffers in a single function call using the iovs parameter.
// Parameters
// fd: The file descriptor (opened with writing permission) to write to.
// iovs: A wasm pointer to an array of __wasi_ciovec_t structures, each describing a buffer to write data from.
// iovs_len: The length of the iovs array.
// nwritten: A wasm pointer to an M::Offset value where the number of bytes written will be written.



// path_create_directory
// Create a directory at a path.
// Description
// The path_create_directory() function creates a new directory at the specified path relative to the given directory. It requires the PATH_CREATE_DIRECTORY right to be set on the directory where the new directory will be created.
// On POSIX systems, a similar functionality is provided by the mkdir() function. It creates a new directory with the given name and the specified permission mode. The mkdir() function is part of the POSIX standard and is widely supported across different platforms.
// Parameters
// fd: The file descriptor representing the directory that the path is relative to.
// path: A wasm pointer to a null-terminated string containing the path data.
// path_len: The length of the path string.


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


// path_remove_directory
// Remove a directory.
// Description
// The path_remove_directory() function removes a directory specified by the given path. It requires the PATH_REMOVE_DIRECTORY right to be set on the directory.
// On POSIX systems, a similar functionality is provided by the rmdir() function. It removes an empty directory with the specified path. The rmdir() function is part of the POSIX standard and is widely supported across different platforms.
// Parameters
// fd: The file descriptor representing the base directory from which the path is resolved.
// path: A wasm pointer to a null-terminated string containing the path of the directory to remove.
// path_len: The length of the path string.


// path_rename
// Rename a file or directory.
// Description
// The path_rename() function renames a file or directory specified by the given path. It requires the PATH_RENAME_SOURCE right on the source directory and the PATH_RENAME_TARGET right on the target directory.
// On POSIX systems, a similar functionality is provided by the rename() function. It renames a file or directory with the specified source and target paths. The rename() function is part of the POSIX standard and is widely supported across different platforms.
// Parameters
// ctx: A mutable reference to the function environment.
// old_fd: The file descriptor representing the base directory for the source path.
// old_path: A wasm pointer to a null-terminated string containing the source path of the file or directory to be renamed.
// old_path_len: The length of the old_path string.
// new_fd: The file descriptor representing the base directory for the target path.
// new_path: A wasm pointer to a null-terminated string containing the target path with the new name for the file or directory.
// new_path_len: The length of the new_path string.


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


// path_unlink_file
// Unlink a file, deleting it if the number of hardlinks is 1.
// Description
// The path_unlink_file() function unlinks a file at the specified path. If the file has only one hardlink (i.e., its link count is 1), it will be deleted from the file system. It requires the PATH_UNLINK_FILE right on the base file descriptor.
// On POSIX systems, a similar functionality is provided by the unlink() function. It removes the specified file from the file system. If the file has no other hardlinks, it is completely deleted. The unlink() function is part of the POSIX standard and is widely supported across different platforms.
// Parameters
// ctx: A mutable reference to the function environment.
// fd: The file descriptor representing the base directory from which the path is understood.
// path: A wasm pointer to a null-terminated string containing the path of the file to be unlinked.
// path_len: The length of the path string.
}