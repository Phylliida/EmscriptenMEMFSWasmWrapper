// Copyright 2021 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.
// syscalls.cpp will implement the syscalls of the new file system replacing the
// old JS version. Current Status: Work in Progress. See
// https://github.com/emscripten-core/emscripten/issues/15041.

#define _LARGEFILE64_SOURCE // For F_GETLK64 etc
#define NDEBUG // prevents boost assert from throwing exceptions and resulting in imports
#define BOOST_DISABLE_ASSERTS
// Copyright 2022 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.
#include <unistd.h> // getentropy
#include <fcntl.h>
#include <assert.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <map>
#include <sys/stat.h>

#include <boost/unordered_map.hpp>

#include <boost/smart_ptr/weak_ptr.hpp>
#include <boost/smart_ptr/make_shared.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>

// stuff done:
// use this custom enable_shared_from_this_no_throw to prevent exception when shared_ptr created
// -fno-exceptions prevents std::map insert from throwing exceptions


namespace boost
{
const boost::detail::sp_nothrow_tag NO_THROW_TAG;
template<class T> class enable_shared_from_this_no_throw
{
protected:

    enable_shared_from_this_no_throw()
    {
    }

    enable_shared_from_this_no_throw(enable_shared_from_this_no_throw const &)
    {
    }

    enable_shared_from_this_no_throw & operator=(enable_shared_from_this_no_throw const &)
    {
        return *this;
    }

    ~enable_shared_from_this_no_throw()
    {
    }

public:

    boost::shared_ptr<T> shared_from_this()
    {
        boost::shared_ptr<T> p( weak_this_, NO_THROW_TAG);
        //BOOST_ASSERT( p.get() == this );
        return p;
    }

    boost::shared_ptr<T const> shared_from_this() const
    {
        boost::shared_ptr<T const> p( weak_this_, NO_THROW_TAG );
        //BOOST_ASSERT( p.get() == this );
        return p;
    }

public: // actually private, but avoids compiler template friendship issues

    // Note: invoked automatically by shared_ptr; do not call
    template<class X, class Y> void _internal_accept_owner( boost::shared_ptr<X> const * ppx, Y * py ) const
    {
        if( weak_this_.expired() )
        {
            weak_this_ = shared_ptr<T>( *ppx, py );
        }
    }

private:

    mutable weak_ptr<T> weak_this_;
};

}

//#include <emscripten/threading.h>
int emscripten_is_main_runtime_thread() { return 1;};


#ifndef NDEBUG
// In debug builds show a message.
namespace wasmfs {
[[noreturn]] void
handle_unreachable(const char* msg, const char* file, unsigned line);
}
#define WASMFS_UNREACHABLE(msg)                                                \
  wasmfs::handle_unreachable(msg, __FILE__, __LINE__)
#else
// In release builds trap in a compact manner.
#define WASMFS_UNREACHABLE(msg) __builtin_trap()
#endif



// Copyright 2022 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

// Copyright 2022 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

// Copyright 2021 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

// This file defines the file object of the new file system.
// Current Status: Work in Progress.
// See https://github.com/emscripten-core/emscripten/issues/15041.


namespace wasmfs {

// Note: The general locking strategy for all Files is to only hold 1 lock at a
// time to prevent deadlock. This methodology can be seen in getDirs().

class Backend;
class Directory;
class Symlink;

// This represents an opaque pointer to a Backend. A user may use this to
// specify a backend in file operations.
using backend_t = Backend*;
const backend_t NullBackend = nullptr;

// Access mode, file creation and file status flags for open.
using oflags_t = uint32_t;

// An abstract representation of an underlying file. All `File` objects
// correspond to underlying (real or conceptual) files in a file system managed
// by some backend, but not all underlying files have a corresponding `File`
// object. For example, a persistent backend may contain some files that have
// not yet been discovered by WasmFS and that therefore do not yet have
// corresponding `File` objects. Backends override the `File` family of classes
// to implement the mapping from `File` objects to their underlying files.
class File : public boost::enable_shared_from_this_no_throw<File> {
public:
  enum FileKind {
    UnknownKind = 0,
    DataFileKind = 1,
    DirectoryKind = 2,
    SymlinkKind = 3
  };

  const FileKind kind;

  template<class T> bool is() const {
    static_assert(std::is_base_of<File, T>::value,
                  "File is not a base of destination type T");
    return int(kind) == int(T::expectedKind);
  }

  template<class T> boost::shared_ptr<T> dynCast() {
    static_assert(std::is_base_of<File, T>::value,
                  "File is not a base of destination type T");
    if (int(kind) == int(T::expectedKind)) {
      return boost::static_pointer_cast<T>(shared_from_this());
    } else {
      return nullptr;
    }
  }

  template<class T> boost::shared_ptr<T> cast() {
    static_assert(std::is_base_of<File, T>::value,
                  "File is not a base of destination type T");
    assert(int(kind) == int(T::expectedKind));
    return boost::static_pointer_cast<T>(shared_from_this());
  }

  ino_t getIno() {
    // Set inode number to the file pointer. This gives a unique inode number.
    // TODO: For security it would be better to use an indirect mapping.
    // Ensure that the pointer will not overflow an ino_t.
    static_assert(sizeof(this) <= sizeof(ino_t));
    return (ino_t)this;
  }

  backend_t getBackend() const { return backend; }

  bool isSeekable() const { return seekable; }

  class Handle;
  Handle locked();

protected:
  File(FileKind kind, mode_t mode, backend_t backend)
    : kind(kind), mode(mode), backend(backend) {
    atime = mtime = ctime = emscripten_date_now();
  }

  // A mutex is needed for multiple accesses to the same file.
  std::recursive_mutex mutex;

  // The size in bytes of a file or return a negative error code. May be
  // called on files that have not been opened.
  virtual off_t getSize() = 0;

  mode_t mode = 0; // User and group mode bits for access permission.

  double atime; // Time when the content was last accessed, in ms.
  double mtime; // Time when the file content was last modified, in ms.
  double ctime; // Time when the file node was last modified, in ms.

  // Reference to parent of current file node. This can be used to
  // traverse up the directory tree. A weak_ptr ensures that the ref
  // count is not incremented. This also ensures that there are no cyclic
  // dependencies where the parent and child have shared_ptrs that reference
  // each other. This prevents the case in which an uncollectable cycle occurs.
  boost::weak_ptr<Directory> parent;

  // This specifies which backend a file is associated with. It may be null
  // (NullBackend) if there is no particular backend associated with the file.
  backend_t backend;

  // By default files are seekable. The rare exceptions are things like pipes
  // and sockets.
  bool seekable = true;
};

class DataFile : public File {
protected:
  // Notify the backend when this file is opened or closed. The backend is
  // responsible for keeping files accessible as long as they are open, even if
  // they are unlinked. Returns 0 on success or a negative error code.
  virtual int open(oflags_t flags) = 0;
  virtual int close() = 0;

  // Return the accessed length or a negative error code. It is not an error to
  // access fewer bytes than requested. Will only be called on opened files.
  // TODO: Allow backends to override the version of read with
  // multiple iovecs to make it possible to implement pipes. See #16269.
  virtual ssize_t read(uint8_t* buf, size_t len, off_t offset) = 0;
  virtual ssize_t write(const uint8_t* buf, size_t len, off_t offset) = 0;

  // Sets the size of the file to a specific size. If new space is allocated, it
  // should be zero-initialized. May be called on files that have not been
  // opened. Returns 0 on success or a negative error code.
  virtual int setSize(off_t size) = 0;

  // Sync the file data to the underlying persistent storage, if any. Returns 0
  // on success or a negative error code.
  virtual int flush() = 0;

public:
  static constexpr FileKind expectedKind = File::DataFileKind;
  DataFile(mode_t mode, backend_t backend)
    : File(File::DataFileKind, mode | S_IFREG, backend) {}
  DataFile(mode_t mode, backend_t backend, mode_t fileType)
    : File(File::DataFileKind, mode | fileType, backend) {}
  virtual ~DataFile() = default;

  class Handle;
  Handle locked();
};

class Directory : public File {
public:
  struct Entry {
    std::string name;
    FileKind kind;
    ino_t ino;
  };

  struct MaybeEntries : std::variant<std::vector<Entry>, int> {
    int getError() {
      if (int* err = std::get_if<int>(this)) {
        assert(*err < 0);
        return *err;
      }
      return 0;
    }

    std::vector<Entry>& operator*() {
      return *std::get_if<std::vector<Entry>>(this);
    }

    std::vector<Entry>* operator->() {
      return std::get_if<std::vector<Entry>>(this);
    }
  };

private:
  // The directory cache, or `dcache`, stores `File` objects for the children of
  // each directory so that subsequent lookups do not need to query the backend.
  // It also supports cross-backend mount point children that are stored
  // exclusively in the cache and not reflected in any backend.
  enum class DCacheKind { Normal, Mount };
  struct DCacheEntry {
    DCacheKind kind;
    boost::shared_ptr<File> file;
  };
  // TODO: Use a cache data structure with smaller code size.
  std::map<std::string, DCacheEntry> dcache;

protected:
  // Return the `File` object corresponding to the file with the given name or
  // null if there is none.
  virtual boost::shared_ptr<File> getChild(const std::string& name) = 0;

  // Inserts a file with the given name, kind, and mode. Returns a `File` object
  // corresponding to the newly created file or nullptr if the new file could
  // not be created. Assumes a child with this name does not already exist.
  // If the operation failed, returns nullptr.
  virtual boost::shared_ptr<DataFile> insertDataFile(const std::string& name,
                                                   mode_t mode) = 0;
  virtual boost::shared_ptr<Directory> insertDirectory(const std::string& name,
                                                     mode_t mode) = 0;
  virtual boost::shared_ptr<Symlink> insertSymlink(const std::string& name,
                                                 const std::string& target) = 0;

  // Move the file represented by `file` from its current directory to this
  // directory with the new `name`, possibly overwriting another file that
  // already exists with that name. The old directory may be the same as this
  // directory. On success return 0 and otherwise return a negative error code
  // without changing any underlying state.
  virtual int insertMove(const std::string& name,
                         boost::shared_ptr<File> file) = 0;

  // Remove the file with the given name. Returns zero on success or if the
  // child has already been removed and otherwise returns a negative error code
  // if the child cannot be removed.
  virtual int removeChild(const std::string& name) = 0;

  // The number of entries in this directory. Returns the number of entries or a
  // negative error code.
  virtual ssize_t getNumEntries() = 0;

  // The list of entries in this directory or a negative error code.
  virtual MaybeEntries getEntries() = 0;

  // Only backends that maintain file identity themselves (see below) need to
  // implement this.
  virtual std::string getName(boost::shared_ptr<File> file) {
    WASMFS_UNREACHABLE("getName unimplemented");
  }

  // Whether this directory implementation always returns the same `File` object
  // for a given file. Most backends can be much simpler if they don't handle
  // this themselves. Instead, they rely on the directory cache (dcache) to
  // maintain file identity for them by ensuring each file is looked up in the
  // backend only once. Some backends, however, already track file identity, so
  // the dcache is not necessary (or would even introduce problems).
  //
  // When this is `true`, backends are responsible for:
  //
  //  1. Ensuring that all insert* and getChild calls returning a particular
  //     file return the same File object.
  //
  //  2. Clearing unlinked Files' parents in `removeChild` and `insertMove`.
  //
  //  3. Implementing `getName`, since it cannot be implemented in terms of the
  //     dcache.
  virtual bool maintainsFileIdentity() { return false; }

public:
  static constexpr FileKind expectedKind = File::DirectoryKind;
  Directory(mode_t mode, backend_t backend)
    : File(File::DirectoryKind, mode | S_IFDIR, backend) {}
  virtual ~Directory() = default;

  class Handle;
  Handle locked();

protected:
  // 4096 bytes is the size of a block in ext4.
  // This value was also copied from the JS file system.
  off_t getSize() override { return 4096; }
};

class Symlink : public File {
public:
  static constexpr FileKind expectedKind = File::SymlinkKind;
  // Note that symlinks provide a mode of 0 to File. The mode of a symlink does
  // not matter, so that value will never be read (what matters is the mode of
  // the target).
  Symlink(backend_t backend) : File(File::SymlinkKind, S_IFLNK, backend) {}
  virtual ~Symlink() = default;

  // Constant, and therefore thread-safe, and can be done without locking.
  virtual std::string getTarget() const = 0;

protected:
  off_t getSize() override { return getTarget().size(); }
};

class File::Handle {
protected:
  // This mutex is needed when one needs to access access a previously locked
  // file in the same thread. For example, rename will need to traverse
  // 2 paths and access the same locked directory twice.
  // TODO: During benchmarking, test recursive vs normal mutex performance.
  std::unique_lock<std::recursive_mutex> lock;
  boost::shared_ptr<File> file;

public:
  Handle(boost::shared_ptr<File> file) : lock(file->mutex), file(file) {}
  Handle(boost::shared_ptr<File> file, std::defer_lock_t)
    : lock(file->mutex, std::defer_lock), file(file) {}
  off_t getSize() { return file->getSize(); }
  mode_t getMode() { return file->mode; }
  void setMode(mode_t mode) {
    // The type bits can never be changed (whether something is a file or a
    // directory, for example).
    file->mode = (file->mode & S_IFMT) | (mode & ~S_IFMT);
  }
  double getCTime() {
    return file->ctime;
  }
  void setCTime(double time) { file->ctime = time; }
  // updateCTime() updates the ctime to the current time.
  void updateCTime() {
    file->ctime = emscripten_date_now();
  }
  double getMTime() {
    return file->mtime;
  }
  void setMTime(double time) { file->mtime = time; }
  // updateMTime() updates the mtime to the current time.
  void updateMTime() {
    file->mtime = emscripten_date_now();
  }
  double getATime() {
    return file->atime;
  }
  void setATime(double time) { file->atime = time; }
  // updateATime() updates the atime to the current time.
  void updateATime() {
    file->atime = emscripten_date_now();
  }

  // Note: parent.lock() creates a new shared_ptr to the same Directory
  // specified by the parent weak_ptr.
  boost::shared_ptr<Directory> getParent() { return file->parent.lock(); }
  void setParent(boost::shared_ptr<Directory> parent) { file->parent = parent; }

  boost::shared_ptr<File> unlocked() { return file; }
};

class DataFile::Handle : public File::Handle {
  boost::shared_ptr<DataFile> getFile() { return file->cast<DataFile>(); }

public:
  Handle(boost::shared_ptr<File> dataFile) : File::Handle(dataFile) {}
  Handle(Handle&&) = default;

  [[nodiscard]] int open(oflags_t flags) { return getFile()->open(flags); }
  [[nodiscard]] int close() { return getFile()->close(); }

  ssize_t read(uint8_t* buf, size_t len, off_t offset) {
    return getFile()->read(buf, len, offset);
  }
  ssize_t write(const uint8_t* buf, size_t len, off_t offset) {
    return getFile()->write(buf, len, offset);
  }

  [[nodiscard]] int setSize(off_t size) { return getFile()->setSize(size); }

  // TODO: Design a proper API for flushing files.
  [[nodiscard]] int flush() { return getFile()->flush(); }

  // This function loads preloaded files from JS Memory into this DataFile.
  // TODO: Make this virtual so specific backends can specialize it for better
  // performance.
  void preloadFromJS(int index);
};

class Directory::Handle : public File::Handle {
  boost::shared_ptr<Directory> getDir() { return file->cast<Directory>(); }
  void cacheChild(const std::string& name,
                  boost::shared_ptr<File> child,
                  DCacheKind kind);

public:
  Handle(boost::shared_ptr<File> directory) : File::Handle(directory) {}
  Handle(boost::shared_ptr<File> directory, std::defer_lock_t)
    : File::Handle(directory, std::defer_lock) {}

  // Retrieve the child if it is in the dcache and otherwise forward the request
  // to the backend, caching any `File` object it returns.
  boost::shared_ptr<File> getChild(const std::string& name);

  // Add a child to this directory's entry cache without actually inserting it
  // in the underlying backend. Assumes a child with this name does not already
  // exist. Return `true` on success and `false` otherwise.
  bool mountChild(const std::string& name, boost::shared_ptr<File> file);

  // Insert a child of the given name, kind, and mode in the underlying backend,
  // which will allocate and return a corresponding `File` on success or return
  // nullptr otherwise. Assumes a child with this name does not already exist.
  // If the operation failed, returns nullptr.
  boost::shared_ptr<DataFile> insertDataFile(const std::string& name,
                                           mode_t mode);
  boost::shared_ptr<Directory> insertDirectory(const std::string& name,
                                             mode_t mode);
  boost::shared_ptr<Symlink> insertSymlink(const std::string& name,
                                         const std::string& target);

  // Move the file represented by `file` from its current directory to this
  // directory with the new `name`, possibly overwriting another file that
  // already exists with that name. The old directory may be the same as this
  // directory. On success return 0 and otherwise return a negative error code
  // without changing any underlying state. This should only be called from
  // renameat with the locks on the old and new parents already held.
  [[nodiscard]] int insertMove(const std::string& name,
                               boost::shared_ptr<File> file);

  // Remove the file with the given name. Returns zero on success or if the
  // child has already been removed and otherwise returns a negative error code
  // if the child cannot be removed.
  [[nodiscard]] int removeChild(const std::string& name);

  std::string getName(boost::shared_ptr<File> file);

  [[nodiscard]] ssize_t getNumEntries();
  [[nodiscard]] MaybeEntries getEntries();
};

inline File::Handle File::locked() { 
   return Handle(shared_from_this());
}

inline DataFile::Handle DataFile::locked() {
    return Handle(shared_from_this());
}

inline Directory::Handle Directory::locked() {
    return Handle(shared_from_this());
}

} // namespace wasmfs

namespace wasmfs::path {

// Typically -ENOTDIR or -ENOENT.
using Error = long;

// The parent directory and the name of an entry within it. The returned string
// view is either backed by the same memory as the view passed to `parseParent`
// or is a view into a static string.
using ParentChild = std::pair<boost::shared_ptr<Directory>, std::string_view>;

// If the path refers to a link, whether we should follow that link. Links among
// the parent directories in the path are always followed.
enum LinkBehavior { FollowLinks, NoFollowLinks };

struct ParsedParent {
private:
  std::variant<Error, ParentChild> val;

public:
  ParsedParent(Error err) : val(err) {}
  ParsedParent(ParentChild pair) : val(pair) {}
  // Always ok to call, returns 0 if there is no error.
  long getError() {
    if (auto* err = std::get_if<Error>(&val)) {
      assert(*err != 0 && "Unexpected zero error value");
      return *err;
    }
    return 0;
  }
  // Call only after checking for an error.
  ParentChild& getParentChild() {
    auto* ptr = std::get_if<ParentChild>(&val);
    assert(ptr && "Unhandled path parsing error!");
    return *ptr;
  }
};

ParsedParent parseParent(std::string_view path, __wasi_fd_t basefd = AT_FDCWD);

struct ParsedFile {
private:
  std::variant<Error, boost::shared_ptr<File>> val;

public:
  ParsedFile(Error err) : val(err) {}
  ParsedFile(boost::shared_ptr<File> file) : val(std::move(file)) {}
  // Always ok to call. Returns 0 if there is no error.
  long getError() {
    if (auto* err = std::get_if<Error>(&val)) {
      assert(*err != 0 && "Unexpected zero error value");
      return *err;
    }
    return 0;
  }
  // Call only after checking for an error.
  boost::shared_ptr<File>& getFile() {
    auto* ptr = std::get_if<boost::shared_ptr<File>>(&val);
    return *ptr;
  }
};

ParsedFile parseFile(std::string_view path,
                     __wasi_fd_t basefd = AT_FDCWD,
                     LinkBehavior links = FollowLinks);

// Like `parseFile`, but handle the cases where flags include `AT_EMPTY_PATH` or
// AT_SYMLINK_NOFOLLOW. Does not validate the flags since different callers have
// different allowed flags.
ParsedFile getFileAt(__wasi_fd_t fd, std::string_view path, int flags);

// Like `parseFile`, but parse the path relative to the given directory.
ParsedFile getFileFrom(boost::shared_ptr<Directory> base, std::string_view path);

} // namespace wasmfs::path





// Copyright 2022 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.


namespace wasmfs::SpecialFiles {

// /dev/null
boost::shared_ptr<DataFile> getNull();

// /dev/stdin
boost::shared_ptr<DataFile> getStdin();

// /dev/stdout
boost::shared_ptr<DataFile> getStdout();

// /dev/stderr
boost::shared_ptr<DataFile> getStderr();

// /dev/random
boost::shared_ptr<DataFile> getRandom();

// /dev/urandom
boost::shared_ptr<DataFile> getURandom();

} // namespace wasmfs::SpecialFiles



// Copyright 2021 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

// This file defines the modular backend abstract class.
// Other file system backends can use this to interface with the new file
// system. Current Status: Work in Progress. See
// https://github.com/emscripten-core/emscripten/issues/15041.



namespace wasmfs {
// A backend (or modular backend) provides a base for the new file system to
// extend its storage capabilities. Files and directories will be represented
// in the file system structure, but their underlying backing could exist in
// persistent storage, another thread, etc.
class Backend {

public:
  virtual boost::shared_ptr<DataFile> createFile(mode_t mode) = 0;
  virtual boost::shared_ptr<Directory> createDirectory(mode_t mode) = 0;
  virtual boost::shared_ptr<Symlink> createSymlink(std::string target) = 0;

  virtual ~Backend() = default;
};

typedef backend_t (*backend_constructor_t)(void*);
} // namespace wasmfs





// Copyright 2021 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

// This file defines the memory file class of the new file system.
// This should be the only backend file type defined in a header since it is the
// default type. Current Status: Work in Progress. See
// https://github.com/emscripten-core/emscripten/issues/15041.

namespace wasmfs {

// This class describes a file that lives in Wasm Memory.
class MemoryDataFile : public DataFile {
  std::vector<uint8_t> buffer;

  int open(oflags_t) override { return 0; }
  int close() override { return 0; }
  ssize_t write(const uint8_t* buf, size_t len, off_t offset) override;
  ssize_t read(uint8_t* buf, size_t len, off_t offset) override;
  int flush() override { return 0; }
  off_t getSize() override { return buffer.size(); }
  int setSize(off_t size) override {
    buffer.resize(size);
    return 0;
  }

public:
  MemoryDataFile(mode_t mode, backend_t backend) : DataFile(mode, backend) {}

  class Handle : public DataFile::Handle {

    boost::shared_ptr<MemoryDataFile> getFile() {
      return file->cast<MemoryDataFile>();
    }

  public:
    Handle(boost::shared_ptr<File> dataFile) : DataFile::Handle(dataFile) {}
  };

  Handle locked() { return Handle(shared_from_this()); }
};

class MemoryDirectory : public Directory {
  // Use a vector instead of a map to save code size.
  struct ChildEntry {
    std::string name;
    boost::shared_ptr<File> child;
  };

  std::vector<ChildEntry> entries;

  std::vector<ChildEntry>::iterator findEntry(const std::string& name);

protected:
  void insertChild(const std::string& name, boost::shared_ptr<File> child) {
    assert(findEntry(name) == entries.end());
    entries.push_back({name, child});
  }

  boost::shared_ptr<File> getChild(const std::string& name) override;

  int removeChild(const std::string& name) override;

  boost::shared_ptr<DataFile> insertDataFile(const std::string& name,
                                           mode_t mode) override {
    auto child = getBackend()->createFile(mode);
    insertChild(name, child);
    return child;
  }

  boost::shared_ptr<Directory> insertDirectory(const std::string& name,
                                             mode_t mode) override {
    return nullptr;
    auto child = getBackend()->createDirectory(mode);
    insertChild(name, child);
    return child;
  }

  boost::shared_ptr<Symlink> insertSymlink(const std::string& name,
                                         const std::string& target) override {
    auto child = getBackend()->createSymlink(target);
    insertChild(name, child);
    return child;
  }

  int insertMove(const std::string& name, boost::shared_ptr<File> file) override;

  ssize_t getNumEntries() override { return entries.size(); }
  Directory::MaybeEntries getEntries() override;

  std::string getName(boost::shared_ptr<File> file) override;

  // Since we internally track files with `File` objects, we don't need the
  // dcache as well.
  bool maintainsFileIdentity() override { return true; }

public:
  MemoryDirectory(mode_t mode, backend_t backend) : Directory(mode, backend) {}
};

class MemorySymlink : public Symlink {
  std::string target;

  std::string getTarget() const override { return target; }

public:
  MemorySymlink(const std::string& target, backend_t backend)
    : Symlink(backend), target(target) {}
};

backend_t createMemoryBackend();

} // namespace wasmfs


// Copyright 2021 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.




#ifdef __cplusplus
extern "C" {
#endif

// These helper functions are defined in library_wasmfs.js.

int _wasmfs_get_num_preloaded_files();
int _wasmfs_get_num_preloaded_dirs();
int _wasmfs_get_preloaded_file_mode(int index);
void _wasmfs_get_preloaded_parent_path(int index, char* parentPath);
void _wasmfs_get_preloaded_path_name(int index, char* fileName);
void _wasmfs_get_preloaded_child_path(int index, char* childName);
size_t _wasmfs_get_preloaded_file_size(uint32_t index);
void _wasmfs_copy_preloaded_file_data(uint32_t index, uint8_t* data);

// Returns the next character from stdin, or -1 on EOF.
int _wasmfs_stdin_get_char(void);

#ifdef __cplusplus
}
#endif



// Copyright 2021 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.
// This file defines the open file table of the new file system.
// Current Status: Work in Progress.
// See https://github.com/emscripten-core/emscripten/issues/15041.

namespace wasmfs {
static_assert(std::is_same<size_t, __wasi_size_t>::value,
              "size_t should be the same as __wasi_size_t");
static_assert(std::is_same<off_t, __wasi_filedelta_t>::value,
              "off_t should be the same as __wasi_filedelta_t");

// Overflow and underflow behaviour are only defined for unsigned types.
template<typename T> bool addWillOverFlow(T a, T b) {
  if (a > 0 && b > std::numeric_limits<T>::max() - a) {
    return true;
  }
  return false;
}

class FileTable;

class OpenFileState : public boost::enable_shared_from_this_no_throw<OpenFileState> {
  boost::shared_ptr<File> file;
  off_t position = 0;
  oflags_t flags; // RD_ONLY, WR_ONLY, RDWR

  // An OpenFileState needs a mutex if there are concurrent accesses on one open
  // file descriptor. This could occur if there are multiple seeks on the same
  // open file descriptor.
  std::recursive_mutex mutex;

  // The number of times this OpenFileState appears in the table. Use this
  // instead of shared_ptr::use_count to avoid accidentally counting temporary
  // objects.
  int uses = 0;

  // We can't make the constructor private because boost::make_shared needs to be
  // able to call it, but we can make it unusable publicly.
  struct private_key {
    explicit private_key(int) {}
  };

  // `uses` is protected by the FileTable lock and can be accessed directly by
  // `FileTable::Handle.
  friend FileTable;

public:
  // Cache directory entries at the moment the directory is opened so that
  // subsequent getdents calls have a stable view of the contents. Including
  // files removed after the open and excluding files added after the open is
  // allowed, and trying to recalculate the directory contents on each getdents
  // call could lead to missed directory entries if there are concurrent
  // deletions that effectively move entries back past the current read position
  // in the open directory.
  const std::vector<Directory::Entry> dirents;

  OpenFileState(private_key,
                oflags_t flags,
                boost::shared_ptr<File> file,
                std::vector<Directory::Entry>&& dirents)
    : file(file), flags(flags), dirents(std::move(dirents)) {}

  [[nodiscard]] static int create(boost::shared_ptr<File> file,
                                  oflags_t flags,
                                  boost::shared_ptr<OpenFileState>& out);

  class Handle {
    boost::shared_ptr<OpenFileState> openFileState;
    std::unique_lock<std::recursive_mutex> lock;

  public:
    Handle(boost::shared_ptr<OpenFileState> openFileState)
      : openFileState(openFileState), lock(openFileState->mutex) {}

    boost::shared_ptr<File>& getFile() { return openFileState->file; };

    off_t getPosition() const { return openFileState->position; };
    void setPosition(off_t pos) { openFileState->position = pos; };

    oflags_t getFlags() const { return openFileState->flags; };
    void setFlags(oflags_t flags) { openFileState->flags = flags; };
  };

  Handle locked() { return Handle(shared_from_this()); }
};

class FileTable {
  // Allow WasmFS to construct the FileTable singleton.
  friend class WasmFS;

  std::vector<boost::shared_ptr<OpenFileState>> entries;
  std::recursive_mutex mutex;

  FileTable();

public:
  // Access to the FileTable must go through a Handle, which holds its lock.
  class Handle {
    FileTable& fileTable;
    std::unique_lock<std::recursive_mutex> lock;

  public:
    Handle(FileTable& fileTable)
      : fileTable(fileTable), lock(fileTable.mutex) {}

    boost::shared_ptr<OpenFileState> getEntry(__wasi_fd_t fd);

    // Set the table slot at `fd` to the given file. If this overwrites the last
    // reference to an OpenFileState for a data file in the table, return the
    // file so it can be closed by the caller. Do not close the file directly in
    // this method so it can be closed later while the FileTable lock is not
    // held.
    [[nodiscard]] boost::shared_ptr<DataFile>
    setEntry(__wasi_fd_t fd, boost::shared_ptr<OpenFileState> openFile);
    __wasi_fd_t addEntry(boost::shared_ptr<OpenFileState> openFileState);
  };

  Handle locked() { return Handle(*this); }
};

} // namespace wasmfs



namespace wasmfs {

class WasmFS {

  std::vector<std::unique_ptr<Backend>> backendTable;
  FileTable fileTable;
  boost::shared_ptr<Directory> rootDirectory;
  boost::shared_ptr<Directory> cwd;
  std::mutex mutex;

  // Private method to initialize root directory once.
  // Initializes default directories including dev/stdin, dev/stdout,
  // dev/stderr. Refers to the same std streams in the open file table.
  boost::shared_ptr<Directory> initRootDirectory();

  // Initialize files specified by --preload-file option.
  void preloadFiles();

public:
  WasmFS();
  ~WasmFS();

  FileTable& getFileTable() { return fileTable; }

  boost::shared_ptr<Directory> getRootDirectory() { return rootDirectory; };

  boost::shared_ptr<Directory> getCWD() {
    const std::lock_guard<std::mutex> lock(mutex);
    return cwd;
  };

  void setCWD(boost::shared_ptr<Directory> directory) {
    const std::lock_guard<std::mutex> lock(mutex);
    cwd = directory;
  };

  backend_t addBackend(std::unique_ptr<Backend> backend) {
    //const std::lock_guard<std::mutex> lock(mutex);
    //backendTable.push_back(std::move(backend));
    //return backendTable.back().get();
    //return ;
  }
};

// Global state instance.
extern WasmFS wasmFS;

} // namespace wasmfs


/*
// File permission macros for wasmfs.
// Used to improve readability compared to those in stat.h
#define WASMFS_PERM_READ 0444

#define WASMFS_PERM_WRITE 0222

#define WASMFS_PERM_EXECUTE 0111

// In Linux, the maximum length for a filename is 255 bytes.
#define WASMFS_NAME_MAX 255

extern "C" {

using namespace wasmfs;

int __syscall_dup3(int oldfd, int newfd, int flags) {
  if (flags & !O_CLOEXEC) {
    // TODO: Test this case.
    return -EINVAL;
  }

  auto fileTable = wasmFS.getFileTable().locked();
  auto oldOpenFile = fileTable.getEntry(oldfd);
  if (!oldOpenFile) {
    return -EBADF;
  }
  if (newfd < 0) {
    return -EBADF;
  }
  if (oldfd == newfd) {
    return -EINVAL;
  }

  // If the file descriptor newfd was previously open, it will just be
  // overwritten silently.
  (void)fileTable.setEntry(newfd, oldOpenFile);
  return newfd;
}

int __syscall_dup(int fd) {
  auto fileTable = wasmFS.getFileTable().locked();

  // Check that an open file exists corresponding to the given fd.
  auto openFile = fileTable.getEntry(fd);
  if (!openFile) {
    return -EBADF;
  }
  return fileTable.addEntry(openFile);
}

// This enum specifies whether file offset will be provided by the open file
// state or provided by argument in the case of pread or pwrite.
enum class OffsetHandling { OpenFileState, Argument };

// Internal write function called by __wasi_fd_write and __wasi_fd_pwrite
// Receives an open file state offset.
// Optionally sets open file state offset.
static __wasi_errno_t writeAtOffset(OffsetHandling setOffset,
                                    __wasi_fd_t fd,
                                    const __wasi_ciovec_t* iovs,
                                    size_t iovs_len,
                                    __wasi_size_t* nwritten,
                                    __wasi_filesize_t offset = 0) {
  auto openFile = wasmFS.getFileTable().locked().getEntry(fd);
  if (!openFile) {
    return __WASI_ERRNO_BADF;
  }

  if (iovs_len < 0 || offset < 0) {
    return __WASI_ERRNO_INVAL;
  }

  auto lockedOpenFile = openFile->locked();
  auto file = lockedOpenFile.getFile()->dynCast<DataFile>();
  if (!file) {
    return __WASI_ERRNO_ISDIR;
  }

  auto lockedFile = file->locked();

  if (setOffset == OffsetHandling::OpenFileState) {
    if (lockedOpenFile.getFlags() & O_APPEND) {
      off_t size = lockedFile.getSize();
      if (size < 0) {
        // Translate to WASI standard of positive return codes.
        return -size;
      }
      offset = size;
      lockedOpenFile.setPosition(offset);
    } else {
      offset = lockedOpenFile.getPosition();
    }
  }

  // TODO: Check open file access mode for write permissions.

  size_t bytesWritten = 0;
  for (size_t i = 0; i < iovs_len; i++) {
    const uint8_t* buf = iovs[i].buf;
    off_t len = iovs[i].buf_len;

    // Check if buf_len specifies a positive length buffer but buf is a
    // null pointer
    if (!buf && len > 0) {
      return __WASI_ERRNO_INVAL;
    }

    // Check if the sum of the buf_len values overflows an off_t (63 bits).
    if (addWillOverFlow(offset, (__wasi_filesize_t)bytesWritten)) {
      return __WASI_ERRNO_FBIG;
    }

    auto result = lockedFile.write(buf, len, offset + bytesWritten);
    if (result < 0) {
      // This individual write failed. Report the error unless we've already
      // written some bytes, in which case report a successful short write.
      if (bytesWritten > 0) {
        break;
      }
      return -result;
    }
    // The write was successful.
    bytesWritten += result;
    if (result < len) {
      // The write was short, so stop here.
      break;
    }
  }
  *nwritten = bytesWritten;
  if (setOffset == OffsetHandling::OpenFileState &&
      lockedOpenFile.getFile()->isSeekable()) {
    lockedOpenFile.setPosition(offset + bytesWritten);
  }
  if (bytesWritten) {
    lockedFile.updateMTime();
  }
  return __WASI_ERRNO_SUCCESS;
}

// Internal read function called by __wasi_fd_read and __wasi_fd_pread
// Receives an open file state offset.
// Optionally sets open file state offset.
// TODO: combine this with writeAtOffset because the code is nearly identical.
static __wasi_errno_t readAtOffset(OffsetHandling setOffset,
                                   __wasi_fd_t fd,
                                   const __wasi_iovec_t* iovs,
                                   size_t iovs_len,
                                   __wasi_size_t* nread,
                                   __wasi_filesize_t offset = 0) {
  auto openFile = wasmFS.getFileTable().locked().getEntry(fd);
  if (!openFile) {
    return __WASI_ERRNO_BADF;
  }

  auto lockedOpenFile = openFile->locked();

  if (setOffset == OffsetHandling::OpenFileState) {
    offset = lockedOpenFile.getPosition();
  }

  if (iovs_len < 0 || offset < 0) {
    return __WASI_ERRNO_INVAL;
  }

  // TODO: Check open file access mode for read permissions.

  auto file = lockedOpenFile.getFile()->dynCast<DataFile>();

  // If file is nullptr, then the file was not a DataFile.
  if (!file) {
    return __WASI_ERRNO_ISDIR;
  }

  auto lockedFile = file->locked();

  size_t bytesRead = 0;
  for (size_t i = 0; i < iovs_len; i++) {
    uint8_t* buf = iovs[i].buf;
    size_t len = iovs[i].buf_len;

    if (!buf && len > 0) {
      return __WASI_ERRNO_INVAL;
    }

    // TODO: Check for overflow when adding offset + bytesRead.
    auto result = lockedFile.read(buf, len, offset + bytesRead);
    if (result < 0) {
      // This individual read failed. Report the error unless we've already read
      // some bytes, in which case report a successful short read.
      if (bytesRead > 0) {
        break;
      }
      return -result;
    }

    // The read was successful.

    // Backends must only return len or less.
    assert(result <= len);

    bytesRead += result;
    if (result < len) {
      // The read was short, so stop here.
      break;
    }
  }
  *nread = bytesRead;
  if (setOffset == OffsetHandling::OpenFileState &&
      lockedOpenFile.getFile()->isSeekable()) {
    lockedOpenFile.setPosition(offset + bytesRead);
  }
  return __WASI_ERRNO_SUCCESS;
}

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



EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_pwrite(__wasi_fd_t fd,
                                const __wasi_ciovec_t* iovs,
                                size_t iovs_len,
                                __wasi_filesize_t offset,
                                __wasi_size_t* nwritten) {
  return writeAtOffset(
    OffsetHandling::Argument, fd, iovs, iovs_len, nwritten, offset);
}

EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_pread(__wasi_fd_t fd,
                               const __wasi_iovec_t* iovs,
                               size_t iovs_len,
                               __wasi_filesize_t offset,
                               __wasi_size_t* nread) {
  return readAtOffset(
    OffsetHandling::Argument, fd, iovs, iovs_len, nread, offset);
}


EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_close(__wasi_fd_t fd) {
  boost::shared_ptr<DataFile> closee;
  {
    // Do not hold the file table lock while performing the close.
    auto fileTable = wasmFS.getFileTable().locked();
    auto entry = fileTable.getEntry(fd);
    if (!entry) {
      return __WASI_ERRNO_BADF;
    }
    closee = fileTable.setEntry(fd, nullptr);
  }
  if (closee) {
    // Translate to WASI standard of positive return codes.
    int ret = -closee->locked().close();
    assert(ret >= 0);
    return ret;
  }
  return __WASI_ERRNO_SUCCESS;
}

EMSCRIPTEN_KEEPALIVE
__wasi_errno_t fd_sync(__wasi_fd_t fd) {
  auto openFile = wasmFS.getFileTable().locked().getEntry(fd);
  if (!openFile) {
    return __WASI_ERRNO_BADF;
  }

  // Nothing to flush for anything but a data file, but also not an error either
  // way. TODO: in the future we may want syncing of directories.
  auto dataFile = openFile->locked().getFile()->dynCast<DataFile>();
  if (dataFile) {
    auto ret = dataFile->locked().flush();
    assert(ret <= 0);
    // Translate to WASI standard of positive return codes.
    return -ret;
  }

  return __WASI_ERRNO_SUCCESS;
}

int __syscall_fdatasync(int fd) {
  // TODO: Optimize this to avoid unnecessarily flushing unnecessary metadata.
  return fd_sync(fd);
}


backend_t wasmfs_get_backend_by_fd(int fd) {
  auto openFile = wasmFS.getFileTable().locked().getEntry(fd);
  if (!openFile) {
    return NullBackend;
  }
  return openFile->locked().getFile()->getBackend();
}

// This function is exposed to users to allow them to obtain a backend_t for a
// specified path.
backend_t wasmfs_get_backend_by_path(const char* path) {
  auto parsed = path::parseFile(path);
  if (parsed.getError()) {
    // Could not find the file.
    return NullBackend;
  }
  return parsed.getFile()->getBackend();
}

static timespec ms_to_timespec(double ms) {
  long long seconds = ms / 1000;
  timespec ts;
  ts.tv_sec = seconds; // seconds
  ts.tv_nsec = (ms - (seconds * 1000)) * 1000 * 1000; // nanoseconds
  return ts;
}

int __syscall_newfstatat(int dirfd, intptr_t path, intptr_t buf, int flags) {
  // Only accept valid flags.
  if (flags & ~(AT_EMPTY_PATH | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW)) {
    // TODO: Test this case.
    return -EINVAL;
  }
  auto parsed = path::getFileAt(dirfd, (char*)path, flags);
  if (auto err = parsed.getError()) {
    return err;
  }
  auto file = parsed.getFile();

  // Extract the information from the file.
  auto lockedFile = file->locked();
  auto buffer = (struct stat*)buf;

  off_t size = lockedFile.getSize();
  if (size < 0) {
    return size;
  }
  buffer->st_size = size;

  // ATTN: hard-coded constant values are copied from the existing JS file
  // system. Specific values were chosen to match existing library_fs.js
  // values.
  // ID of device containing file: Hardcode 1 for now, no meaning at the
  // moment for Emscripten.
  buffer->st_dev = 1;
  buffer->st_mode = lockedFile.getMode();
  buffer->st_ino = file->getIno();
  // The number of hard links is 1 since they are unsupported.
  buffer->st_nlink = 1;
  buffer->st_uid = 0;
  buffer->st_gid = 0;
  // Device ID (if special file) No meaning right now for Emscripten.
  buffer->st_rdev = 0;
  // The syscall docs state this is hardcoded to # of 512 byte blocks.
  buffer->st_blocks = (buffer->st_size + 511) / 512;
  // Specifies the preferred blocksize for efficient disk I/O.
  buffer->st_blksize = 4096;
  buffer->st_atim = ms_to_timespec(lockedFile.getATime());
  buffer->st_mtim = ms_to_timespec(lockedFile.getMTime());
  buffer->st_ctim = ms_to_timespec(lockedFile.getCTime());
  return __WASI_ERRNO_SUCCESS;
}

int __syscall_stat64(intptr_t path, intptr_t buf) {
  return __syscall_newfstatat(AT_FDCWD, path, buf, 0);
}

int __syscall_lstat64(intptr_t path, intptr_t buf) {
  return __syscall_newfstatat(AT_FDCWD, path, buf, AT_SYMLINK_NOFOLLOW);
}

int __syscall_fstat64(int fd, intptr_t buf) {
  return __syscall_newfstatat(fd, (intptr_t) "", buf, AT_EMPTY_PATH);
}

// When calling doOpen(), we may request an FD be returned, or we may not need
// that return value (in which case no FD need be allocated, and we return 0 on
// success).
enum class OpenReturnMode { FD, Nothing };

static __wasi_fd_t doOpen(path::ParsedParent parsed,
                          int flags,
                          mode_t mode,
                          backend_t backend = NullBackend,
                          OpenReturnMode returnMode = OpenReturnMode::FD) {
  
  int accessMode = (flags & O_ACCMODE);
  if (accessMode != O_WRONLY && accessMode != O_RDONLY &&
      accessMode != O_RDWR) {
    return -EINVAL;
  }

  // TODO: remove assert when all functionality is complete.
  assert((flags & ~(O_CREAT | O_EXCL | O_DIRECTORY | O_TRUNC | O_APPEND |
                    O_RDWR | O_WRONLY | O_RDONLY | O_LARGEFILE | O_NOFOLLOW |
                    O_CLOEXEC | O_NONBLOCK)) == 0);

  if (auto err = parsed.getError()) {
    return err;
  }
  auto& [parent, childName] = parsed.getParentChild();
  if (childName.size() > WASMFS_NAME_MAX) {
    return -ENAMETOOLONG;
  }

  boost::shared_ptr<File> child;
  {
    auto lockedParent = parent->locked();
    child = lockedParent.getChild(std::string(childName));
    // The requested node was not found.
    if (!child) {
      // If curr is the last element and the create flag is specified
      // If O_DIRECTORY is also specified, still create a regular file:
      // https://man7.org/linux/man-pages/man2/open.2.html#BUGS
      if (!(flags & O_CREAT)) {
        return -ENOENT;
      }

      // Inserting into an unlinked directory is not allowed.
      if (!lockedParent.getParent()) {
        return -ENOENT;
      }

      // Mask out everything except the permissions bits.
      mode &= S_IALLUGO;

      // If there is no explicitly provided backend, use the parent's backend.
      if (!backend) {
        backend = parent->getBackend();
      }

      // TODO: Check write permissions on the parent directory.
      boost::shared_ptr<File> created;
      if (backend == parent->getBackend()) {
        created = lockedParent.insertDataFile(std::string(childName), mode);
        if (!created) {
          // TODO Receive a specific error code, and report it here. For now,
          //      report a generic error.
          return -EIO;
        }
      } else {
        created = backend->createFile(mode);
        if (!created) {
          // TODO Receive a specific error code, and report it here. For now,
          //      report a generic error.
          return -EIO;
        }
        [[maybe_unused]] bool mounted =
          lockedParent.mountChild(std::string(childName), created);
        assert(mounted);
      }
      // TODO: Check that the insert actually succeeds.
      if (returnMode == OpenReturnMode::Nothing) {
        return 0;
      }

      boost::shared_ptr<OpenFileState> openFile;
      if (auto err = OpenFileState::create(created, flags, openFile)) {
        assert(err < 0);
        return err;
      }
      return wasmFS.getFileTable().locked().addEntry(openFile);
    }
  }

  if (auto link = child->dynCast<Symlink>()) {
    if (flags & O_NOFOLLOW) {
      return -ELOOP;
    }
    // TODO: The link dereference count starts back at 0 here. We could
    // propagate it from the previous path parsing instead.
    auto target = link->getTarget();
    auto parsedLink = path::getFileFrom(parent, target);
    if (auto err = parsedLink.getError()) {
      return err;
    }
    child = parsedLink.getFile();
  }
  assert(!child->is<Symlink>());

  // Return an error if the file exists and O_CREAT and O_EXCL are specified.
  if ((flags & O_EXCL) && (flags & O_CREAT)) {
    return -EEXIST;
  }

  if (child->is<Directory>() && accessMode != O_RDONLY) {
    return -EISDIR;
  }

  // Check user permissions.
  auto fileMode = child->locked().getMode();
  if ((accessMode == O_RDONLY || accessMode == O_RDWR) &&
      !(fileMode & WASMFS_PERM_READ)) {
    return -EACCES;
  }
  if ((accessMode == O_WRONLY || accessMode == O_RDWR) &&
      !(fileMode & WASMFS_PERM_WRITE)) {
    return -EACCES;
  }

  // Fail if O_DIRECTORY is specified and pathname is not a directory
  if (flags & O_DIRECTORY && !child->is<Directory>()) {
    return -ENOTDIR;
  }

  // Note that we open the file before truncating it because some backends may
  // truncate opened files more efficiently (e.g. OPFS).
  boost::shared_ptr<OpenFileState> openFile;
  if (auto err = OpenFileState::create(child, flags, openFile)) {
    assert(err < 0);
    return err;
  }

  // If O_TRUNC, truncate the file if possible.
  if (flags & O_TRUNC) {
    if (!child->is<DataFile>()) {
      return -EISDIR;
    }
    if ((fileMode & WASMFS_PERM_WRITE) == 0) {
      return -EACCES;
    }
    // Try to truncate the file, continuing silently if we cannot.
    (void)child->cast<DataFile>()->locked().setSize(0);
  }

  return wasmFS.getFileTable().locked().addEntry(openFile);
}

// This function is exposed to users and allows users to create a file in a
// specific backend. An fd to an open file is returned.
EMSCRIPTEN_KEEPALIVE
int create_file(char* pathname, mode_t mode, backend_t backend) {
  static_assert(std::is_same_v<decltype(doOpen(0, 0, 0, 0)), unsigned int>,
                "unexpected conversion from result of doOpen to int");
  return doOpen(
    path::parseParent((char*)pathname), O_CREAT | O_EXCL, mode, backend);
}
// TODO: Test this with non-AT_FDCWD values.
int __syscall_openat(int dirfd, intptr_t path, int flags, ...) {
  mode_t mode = 0;
  va_list v1;
  va_start(v1, flags);
  mode = va_arg(v1, int);
  va_end(v1);

  return doOpen(path::parseParent((char*)path, dirfd), flags, mode);
}

int __syscall_mknodat(int dirfd, intptr_t path, int mode, int dev) {
  assert(dev == 0); // TODO: support special devices
  if (mode & S_IFDIR) {
    return -EINVAL;
  }
  if (mode & S_IFIFO) {
    return -EPERM;
  }
  return doOpen(path::parseParent((char*)path, dirfd),
                O_CREAT | O_EXCL,
                mode,
                NullBackend,
                OpenReturnMode::Nothing);
}

static int
doMkdir(path::ParsedParent parsed, int mode, backend_t backend = NullBackend) {
  if (auto err = parsed.getError()) {
    return err;
  }
  auto& [parent, childNameView] = parsed.getParentChild();
  std::string childName(childNameView);
  auto lockedParent = parent->locked();

  if (childName.size() > WASMFS_NAME_MAX) {
    return -ENAMETOOLONG;
  }

  // Check if the requested directory already exists.
  if (lockedParent.getChild(childName)) {
    return -EEXIST;
  }

  // Mask rwx permissions for user, group and others, and the sticky bit.
  // This prevents users from entering S_IFREG for example.
  // https://www.gnu.org/software/libc/manual/html_node/Permission-Bits.html
  mode &= S_IRWXUGO | S_ISVTX;

  if (!(lockedParent.getMode() & WASMFS_PERM_WRITE)) {
    return -EACCES;
  }

  // By default, the backend that the directory is created in is the same as
  // the parent directory. However, if a backend is passed as a parameter,
  // then that backend is used.
  if (!backend) {
    backend = parent->getBackend();
  }

  if (backend == parent->getBackend()) {
    if (!lockedParent.insertDirectory(childName, mode)) {
      // TODO Receive a specific error code, and report it here. For now, report
      //      a generic error.
      return -EIO;
    }
  } else {
    auto created = backend->createDirectory(mode);
    if (!created) {
      // TODO Receive a specific error code, and report it here. For now, report
      //      a generic error.
      return -EIO;
    }
    [[maybe_unused]] bool mounted = lockedParent.mountChild(childName, created);
    assert(mounted);
  }

  // TODO: Check that the insertion is successful.

  return 0;
}

// This function is exposed to users and allows users to specify a particular
// backend that a directory should be created within.
int wasmfs_create_directory(char* path, int mode, backend_t backend) {
  static_assert(std::is_same_v<decltype(doMkdir(0, 0, 0)), int>,
                "unexpected conversion from result of doMkdir to int");
  return doMkdir(path::parseParent(path), mode, backend);
}

// TODO: Test this.
int __syscall_mkdirat(int dirfd, intptr_t path, int mode) {
  return doMkdir(path::parseParent((char*)path, dirfd), mode);
}

__wasi_errno_t __wasi_fd_seek(__wasi_fd_t fd,
                              __wasi_filedelta_t offset,
                              __wasi_whence_t whence,
                              __wasi_filesize_t* newoffset) {
  auto openFile = wasmFS.getFileTable().locked().getEntry(fd);
  if (!openFile) {
    return __WASI_ERRNO_BADF;
  }
  auto lockedOpenFile = openFile->locked();

  if (!lockedOpenFile.getFile()->isSeekable()) {
    return __WASI_ERRNO_SPIPE;
  }

  off_t position;
  if (whence == SEEK_SET) {
    position = offset;
  } else if (whence == SEEK_CUR) {
    position = lockedOpenFile.getPosition() + offset;
  } else if (whence == SEEK_END) {
    // Only the open file state is altered in seek. Locking the underlying
    // data file here once is sufficient.
    off_t size = lockedOpenFile.getFile()->locked().getSize();
    if (size < 0) {
      // Translate to WASI standard of positive return codes.
      return -size;
    }
    position = size + offset;
  } else {
    return __WASI_ERRNO_INVAL;
  }

  if (position < 0) {
    return __WASI_ERRNO_INVAL;
  }

  lockedOpenFile.setPosition(position);

  if (newoffset) {
    *newoffset = position;
  }

  return __WASI_ERRNO_SUCCESS;
}

static int doChdir(boost::shared_ptr<File>& file) {
  auto dir = file->dynCast<Directory>();
  if (!dir) {
    return -ENOTDIR;
  }
  wasmFS.setCWD(dir);
  return 0;
}

int __syscall_chdir(intptr_t path) {
  auto parsed = path::parseFile((char*)path);
  if (auto err = parsed.getError()) {
    return err;
  }
  return doChdir(parsed.getFile());
}

int __syscall_fchdir(int fd) {
  auto openFile = wasmFS.getFileTable().locked().getEntry(fd);
  if (!openFile) {
    return -EBADF;
  }
  return doChdir(openFile->locked().getFile());
}

int __syscall_getcwd(intptr_t buf, size_t size) {
  // Check if buf points to a bad address.
  if (!buf && size > 0) {
    return -EFAULT;
  }

  // Check if the size argument is zero and buf is not a null pointer.
  if (buf && size == 0) {
    return -EINVAL;
  }

  auto curr = wasmFS.getCWD();

  std::string result = "";

  while (curr != wasmFS.getRootDirectory()) {
    auto parent = curr->locked().getParent();
    // Check if the parent exists. The parent may not exist if the CWD or one
    // of its ancestors has been unlinked.
    if (!parent) {
      return -ENOENT;
    }

    auto parentDir = parent->dynCast<Directory>();

    auto name = parentDir->locked().getName(curr);
    result = '/' + name + result;
    curr = parentDir;
  }

  // Check if the cwd is the root directory.
  if (result.empty()) {
    result = "/";
  }

  auto res = result.c_str();
  int len = strlen(res) + 1;

  // Check if the size argument is less than the length of the absolute
  // pathname of the working directory, including null terminator.
  if (len >= size) {
    return -ERANGE;
  }

  // Return value is a null-terminated c string.
  strcpy((char*)buf, res);

  return len;
}

__wasi_errno_t __wasi_fd_fdstat_get(__wasi_fd_t fd, __wasi_fdstat_t* stat) {
  // TODO: This is only partial implementation of __wasi_fd_fdstat_get. Enough
  // to get __wasi_fd_is_valid working.
  // There are other fields in the stat structure that we should really
  // be filling in here.
  auto openFile = wasmFS.getFileTable().locked().getEntry(fd);
  if (!openFile) {
    return __WASI_ERRNO_BADF;
  }

  if (openFile->locked().getFile()->is<Directory>()) {
    stat->fs_filetype = __WASI_FILETYPE_DIRECTORY;
  } else {
    stat->fs_filetype = __WASI_FILETYPE_REGULAR_FILE;
  }
  return __WASI_ERRNO_SUCCESS;
}

// TODO: Test this with non-AT_FDCWD values.
int __syscall_unlinkat(int dirfd, intptr_t path, int flags) {
  if (flags & ~AT_REMOVEDIR) {
    // TODO: Test this case.
    return -EINVAL;
  }
  // It is invalid for rmdir paths to end in ".", but we need to distinguish
  // this case from the case of `parseParent` returning (root, '.') when parsing
  // "/", so we need to find the invalid "/." manually.
  if (flags == AT_REMOVEDIR) {
    std::string_view p((char*)path);
    // Ignore trailing '/'.
    while (!p.empty() && p.back() == '/') {
      p.remove_suffix(1);
    }
    if (p.size() >= 2 && p.substr(p.size() - 2) == std::string_view("/.")) {
      return -EINVAL;
    }
  }
  auto parsed = path::parseParent((char*)path, dirfd);
  if (auto err = parsed.getError()) {
    return err;
  }
  auto& [parent, childNameView] = parsed.getParentChild();
  std::string childName(childNameView);
  auto lockedParent = parent->locked();
  auto file = lockedParent.getChild(childName);
  if (!file) {
    return -ENOENT;
  }
  // Disallow removing the root directory, even if it is empty.
  if (file == wasmFS.getRootDirectory()) {
    return -EBUSY;
  }

  auto lockedFile = file->locked();
  if (auto dir = file->dynCast<Directory>()) {
    if (flags != AT_REMOVEDIR) {
      return -EISDIR;
    }
    // A directory can only be removed if it has no entries.
    if (dir->locked().getNumEntries() > 0) {
      return -ENOTEMPTY;
    }
  } else {
    // A normal file or symlink.
    if (flags == AT_REMOVEDIR) {
      return -ENOTDIR;
    }
  }

  // Cannot unlink/rmdir if the parent dir doesn't have write permissions.
  if (!(lockedParent.getMode() & WASMFS_PERM_WRITE)) {
    return -EACCES;
  }

  // Input is valid, perform the unlink.
  return lockedParent.removeChild(childName);
}

int __syscall_rmdir(intptr_t path) {
  return __syscall_unlinkat(AT_FDCWD, path, AT_REMOVEDIR);
}

// wasmfs_unmount is similar to __syscall_unlinkat, but assumes AT_REMOVEDIR is
// true and will only unlink mountpoints (Empty and nonempty).
int wasmfs_unmount(intptr_t path) {
  auto parsed = path::parseParent((char*)path, AT_FDCWD);
  if (auto err = parsed.getError()) {
    return err;
  }
  auto& [parent, childNameView] = parsed.getParentChild();
  std::string childName(childNameView);
  auto lockedParent = parent->locked();
  auto file = lockedParent.getChild(childName);
  if (!file) {
    return -ENOENT;
  }
  // Disallow removing the root directory, even if it is empty.
  if (file == wasmFS.getRootDirectory()) {
    return -EBUSY;
  }

  if (auto dir = file->dynCast<Directory>()) {
    if (parent->getBackend() == dir->getBackend()) {
      // The child is not a valid mountpoint.
      return -EINVAL;
    }
  } else {
    // A normal file or symlink.
    return -ENOTDIR;
  }

  // Input is valid, perform the unlink.
  return lockedParent.removeChild(childName);
}

int __syscall_getdents64(int fd, intptr_t dirp, size_t count) {
  dirent* result = (dirent*)dirp;

  // Check if the result buffer is too small.
  if (count / sizeof(dirent) == 0) {
    return -EINVAL;
  }

  auto openFile = wasmFS.getFileTable().locked().getEntry(fd);
  if (!openFile) {
    return -EBADF;
  }
  auto lockedOpenFile = openFile->locked();

  auto dir = lockedOpenFile.getFile()->dynCast<Directory>();
  if (!dir) {
    return -ENOTDIR;
  }
  auto lockedDir = dir->locked();

  // A directory's position corresponds to the index in its entries vector.
  int index = lockedOpenFile.getPosition();

  // If this directory has been unlinked and has no parent, then it is
  // completely empty.
  auto parent = lockedDir.getParent();
  if (!parent) {
    return 0;
  }

  off_t bytesRead = 0;
  const auto& dirents = openFile->dirents;
  for (; index < dirents.size() && bytesRead + sizeof(dirent) <= count;
       index++) {
    const auto& entry = dirents[index];
    result->d_ino = entry.ino;
    result->d_off = index + 1;
    result->d_reclen = sizeof(dirent);
    switch (entry.kind) {
      case File::UnknownKind:
        result->d_type = DT_UNKNOWN;
        break;
      case File::DataFileKind:
        result->d_type = DT_REG;
        break;
      case File::DirectoryKind:
        result->d_type = DT_DIR;
        break;
      case File::SymlinkKind:
        result->d_type = DT_LNK;
        break;
      default:
        result->d_type = DT_UNKNOWN;
        break;
    }
    assert(entry.name.size() + 1 <= sizeof(result->d_name));
    strcpy(result->d_name, entry.name.c_str());
    ++result;
    bytesRead += sizeof(dirent);
  }

  // Update position
  lockedOpenFile.setPosition(index);

  return bytesRead;
}

// TODO: Test this with non-AT_FDCWD values.
int __syscall_renameat(int olddirfd,
                       intptr_t oldpath,
                       int newdirfd,
                       intptr_t newpath) {
  // Rename is the only syscall that needs to (or is allowed to) acquire locks
  // on two directories at once. It requires locks on both the old and new
  // parent directories to ensure that the moved file can be atomically removed
  // from the old directory and added to the new directory without something
  // changing that would prevent the move.
  //
  // To prevent deadlock in the case of simultaneous renames, serialize renames
  // with an additional global lock.
  static std::mutex renameMutex;
  std::lock_guard<std::mutex> renameLock(renameMutex);

  // Get the old directory.
  auto parsedOld = path::parseParent((char*)oldpath, olddirfd);
  if (auto err = parsedOld.getError()) {
    return err;
  }
  auto& [oldParent, oldFileNameView] = parsedOld.getParentChild();
  std::string oldFileName(oldFileNameView);

  // Get the new directory.
  auto parsedNew = path::parseParent((char*)newpath, newdirfd);
  if (auto err = parsedNew.getError()) {
    return err;
  }
  auto& [newParent, newFileNameView] = parsedNew.getParentChild();
  std::string newFileName(newFileNameView);

  if (newFileNameView.size() > WASMFS_NAME_MAX) {
    return -ENAMETOOLONG;
  }

  // Lock both directories.
  auto lockedOldParent = oldParent->locked();
  auto lockedNewParent = newParent->locked();

  // Get the source and destination files.
  auto oldFile = lockedOldParent.getChild(oldFileName);
  auto newFile = lockedNewParent.getChild(newFileName);

  if (!oldFile) {
    return -ENOENT;
  }

  // If the source and destination are the same, do nothing.
  if (oldFile == newFile) {
    return 0;
  }

  // Never allow renaming or overwriting the root.
  auto root = wasmFS.getRootDirectory();
  if (oldFile == root || newFile == root) {
    return -EBUSY;
  }

  // Cannot modify either directory without write permissions.
  if (!(lockedOldParent.getMode() & WASMFS_PERM_WRITE) ||
      !(lockedNewParent.getMode() & WASMFS_PERM_WRITE)) {
    return -EACCES;
  }

  // Both parents must have the same backend.
  if (oldParent->getBackend() != newParent->getBackend()) {
    return -EXDEV;
  }

  // Check that oldDir is not an ancestor of newDir.
  for (auto curr = newParent; curr != root; curr = curr->locked().getParent()) {
    if (curr == oldFile) {
      return -EINVAL;
    }
  }

  // The new file will be removed if it already exists.
  if (newFile) {
    if (auto newDir = newFile->dynCast<Directory>()) {
      // Cannot overwrite a directory with a non-directory.
      auto oldDir = oldFile->dynCast<Directory>();
      if (!oldDir) {
        return -EISDIR;
      }
      // Cannot overwrite a non-empty directory.
      if (newDir->locked().getNumEntries() > 0) {
        return -ENOTEMPTY;
      }
    } else {
      // Cannot overwrite a non-directory with a directory.
      if (oldFile->is<Directory>()) {
        return -ENOTDIR;
      }
    }
  }

  // Perform the move.
  if (auto err = lockedNewParent.insertMove(newFileName, oldFile)) {
    assert(err < 0);
    return err;
  }
  return 0;
}

// TODO: Test this with non-AT_FDCWD values.
int __syscall_symlinkat(intptr_t target, int newdirfd, intptr_t linkpath) {
  auto parsed = path::parseParent((char*)linkpath, newdirfd);
  if (auto err = parsed.getError()) {
    return err;
  }
  auto& [parent, childNameView] = parsed.getParentChild();
  if (childNameView.size() > WASMFS_NAME_MAX) {
    return -ENAMETOOLONG;
  }
  auto lockedParent = parent->locked();
  std::string childName(childNameView);
  if (lockedParent.getChild(childName)) {
    return -EEXIST;
  }
  if (!lockedParent.insertSymlink(childName, (char*)target)) {
    return -EPERM;
  }
  return 0;
}

int __syscall_symlink(intptr_t target, intptr_t linkpath) {
  return __syscall_symlinkat(target, AT_FDCWD, linkpath);
}

// TODO: Test this with non-AT_FDCWD values.
int __syscall_readlinkat(int dirfd,
                         intptr_t path,
                         intptr_t buf,
                         size_t bufsize) {
  // TODO: Handle empty paths.
  auto parsed = path::parseFile((char*)path, dirfd, path::NoFollowLinks);
  if (auto err = parsed.getError()) {
    return err;
  }
  auto link = parsed.getFile()->dynCast<Symlink>();
  if (!link) {
    return -EINVAL;
  }
  const auto& target = link->getTarget();
  auto bytes = std::min((size_t)bufsize, target.size());
  memcpy((char*)buf, target.c_str(), bytes);
  return bytes;
}

static double timespec_to_ms(timespec ts) {
  if (ts.tv_nsec == UTIME_OMIT) {
    return INFINITY;
  }
  if (ts.tv_nsec == UTIME_NOW) {
    return emscripten_date_now();
  }
  return double(ts.tv_sec) * 1000 + double(ts.tv_nsec) / (1000 * 1000);
}

// TODO: Test this with non-AT_FDCWD values.
int __syscall_utimensat(int dirFD, intptr_t path_, intptr_t times_, int flags) {
  const char* path = (const char*)path_;
  const struct timespec* times = (const struct timespec*)times_;
  if (flags & ~AT_SYMLINK_NOFOLLOW) {
    // TODO: Test this case.
    return -EINVAL;
  }

  // Add AT_EMPTY_PATH as Linux (and so, musl, and us) has a nonstandard
  // behavior in which an empty path means to operate on whatever is in dirFD
  // (directory or not), which is exactly the behavior of AT_EMPTY_PATH (but
  // without passing that in). See "C library/kernel ABI differences" in
  // https://man7.org/linux/man-pages/man2/utimensat.2.html
  //
  // TODO: Handle AT_SYMLINK_NOFOLLOW once we traverse symlinks correctly.
  auto parsed = path::getFileAt(dirFD, path, flags | AT_EMPTY_PATH);
  if (auto err = parsed.getError()) {
    return err;
  }

  // TODO: Handle tv_nsec being UTIME_NOW or UTIME_OMIT.
  // TODO: Check for write access to the file (see man page for specifics).
  double aTime, mTime;

  if (times == nullptr) {
    aTime = mTime = emscripten_date_now();
  } else {
    aTime = timespec_to_ms(times[0]);
    mTime = timespec_to_ms(times[1]);
  }

  auto locked = parsed.getFile()->locked();
  if (aTime != INFINITY) {
    locked.setATime(aTime);
  }
  if (mTime != INFINITY) {
    locked.setMTime(mTime);
  }

  return 0;
}

// TODO: Test this with non-AT_FDCWD values.
int __syscall_fchmodat2(int dirfd, intptr_t path, int mode, int flags) {
  if (flags & ~AT_SYMLINK_NOFOLLOW) {
    // TODO: Test this case.
    return -EINVAL;
  }
  auto parsed = path::getFileAt(dirfd, (char*)path, flags);
  if (auto err = parsed.getError()) {
    return err;
  }
  auto lockedFile = parsed.getFile()->locked();
  lockedFile.setMode(mode);
  // On POSIX, ctime is updated on metadata changes, like chmod.
  lockedFile.updateCTime();
  return 0;
}

int __syscall_chmod(intptr_t path, int mode) {
  return __syscall_fchmodat2(AT_FDCWD, path, mode, 0);
}

int __syscall_fchmod(int fd, int mode) {
  auto openFile = wasmFS.getFileTable().locked().getEntry(fd);
  if (!openFile) {
    return -EBADF;
  }
  auto lockedFile = openFile->locked().getFile()->locked();
  lockedFile.setMode(mode);
  lockedFile.updateCTime();
  return 0;
}

int __syscall_fchownat(
  int dirfd, intptr_t path, int owner, int group, int flags) {
  // Only accept valid flags.
  if (flags & ~(AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW)) {
    // TODO: Test this case.
    return -EINVAL;
  }
  auto parsed = path::getFileAt(dirfd, (char*)path, flags);
  if (auto err = parsed.getError()) {
    return err;
  }

  // Ignore the actual owner and group because we don't track those.
  // TODO: Update metadata time stamp.
  return 0;
}

int __syscall_fchown32(int fd, int owner, int group) {
  return __syscall_fchownat(fd, (intptr_t) "", owner, group, AT_EMPTY_PATH);
}

// TODO: Test this with non-AT_FDCWD values.
int __syscall_faccessat(int dirfd, intptr_t path, int amode, int flags) {
  // The input must be F_OK (check for existence) or a combination of [RWX]_OK
  // flags.
  if (amode != F_OK && (amode & ~(R_OK | W_OK | X_OK))) {
    return -EINVAL;
  }
  if (flags & ~(AT_EACCESS | AT_SYMLINK_NOFOLLOW)) {
    // TODO: Test this case.
    return -EINVAL;
  }

  // TODO: Handle AT_SYMLINK_NOFOLLOW once we traverse symlinks correctly.
  auto parsed = path::parseFile((char*)path, dirfd);
  if (auto err = parsed.getError()) {
    return err;
  }

  if (amode != F_OK) {
    auto mode = parsed.getFile()->locked().getMode();
    if ((amode & R_OK) && !(mode & WASMFS_PERM_READ)) {
      return -EACCES;
    }
    if ((amode & W_OK) && !(mode & WASMFS_PERM_WRITE)) {
      return -EACCES;
    }
    if ((amode & X_OK) && !(mode & WASMFS_PERM_EXECUTE)) {
      return -EACCES;
    }
  }

  return 0;
}


static int doTruncate(boost::shared_ptr<File>& file, off_t size) {
  auto dataFile = file->dynCast<DataFile>();

  if (!dataFile) {
    return -EISDIR;
  }

  auto locked = dataFile->locked();
  if (!(locked.getMode() & WASMFS_PERM_WRITE)) {
    return -EACCES;
  }

  if (size < 0) {
    return -EINVAL;
  }

  int ret = locked.setSize(size);
  assert(ret <= 0);
  return ret;
}

int __syscall_truncate64(intptr_t path, off_t size) {
  auto parsed = path::parseFile((char*)path);
  if (auto err = parsed.getError()) {
    return err;
  }
  return doTruncate(parsed.getFile(), size);
}

int __syscall_ftruncate64(int fd, off_t size) {
  auto openFile = wasmFS.getFileTable().locked().getEntry(fd);
  if (!openFile) {
    return -EBADF;
  }
  auto ret = doTruncate(openFile->locked().getFile(), size);
  // XXX It is not clear from the docs why ftruncate would differ from
  //     truncate here. However, on Linux this definitely happens, and the old
  //     FS matches that as well, so do the same here.
  if (ret == -EACCES) {
    ret = -EINVAL;
  }
  return ret;
}

static bool isTTY(boost::shared_ptr<File>& file) {
  // TODO: Full TTY support. For now, just see stdin/out/err as terminals and
  //       nothing else.
  return file == SpecialFiles::getStdin() ||
         file == SpecialFiles::getStdout() || file == SpecialFiles::getStderr();
}

int __syscall_ioctl(int fd, int request, ...) {
  auto openFile = wasmFS.getFileTable().locked().getEntry(fd);
  if (!openFile) {
    return -EBADF;
  }
  if (!isTTY(openFile->locked().getFile())) {
    return -ENOTTY;
  }
  // TODO: Full TTY support. For now this is limited, and matches the old FS.
  switch (request) {
    case TCGETA:
    case TCGETS:
    case TCSETA:
    case TCSETAW:
    case TCSETAF:
    case TCSETS:
    case TCSETSW:
    case TCSETSF:
    case TIOCGWINSZ:
    case TIOCSWINSZ: {
      // TTY operations that we do nothing for anyhow can just be ignored.
      return 0;
    }
    default: {
      return -EINVAL; // not supported
    }
  }
}

int __syscall_pipe(intptr_t fd) {
  auto* fds = (__wasi_fd_t*)fd;

  // Make a pipe: Two PipeFiles that share a single data source between them, so
  // that writing to one can be read in the other.
  //
  // No backend is needed here, so pass in nullptr for that.
  auto data = boost::make_shared<PipeData>();
  auto reader = boost::make_shared<PipeFile>(S_IRUGO, data);
  auto writer = boost::make_shared<PipeFile>(S_IWUGO, data);

  boost::shared_ptr<OpenFileState> openReader, openWriter;
  (void)OpenFileState::create(reader, O_RDONLY, openReader);
  (void)OpenFileState::create(writer, O_WRONLY, openWriter);

  auto fileTable = wasmFS.getFileTable().locked();
  fds[0] = fileTable.addEntry(openReader);
  fds[1] = fileTable.addEntry(openWriter);

  return 0;
}

// int poll(struct pollfd* fds, nfds_t nfds, int timeout);
int __syscall_poll(intptr_t fds_, int nfds, int timeout) {
  struct pollfd* fds = (struct pollfd*)fds_;
  auto fileTable = wasmFS.getFileTable().locked();

  // Process the list of FDs and compute their revents masks. Count the number
  // of nonzero such masks, which is our return value.
  int nonzero = 0;
  for (nfds_t i = 0; i < nfds; i++) {
    auto* pollfd = &fds[i];
    auto fd = pollfd->fd;
    if (fd < 0) {
      // Negative FDs are ignored in poll().
      pollfd->revents = 0;
      continue;
    }
    // Assume invalid, unless there is an open file.
    auto mask = POLLNVAL;
    auto openFile = fileTable.getEntry(fd);
    if (openFile) {
      mask = 0;
      auto flags = openFile->locked().getFlags();
      auto accessMode = flags & O_ACCMODE;
      auto readBit = pollfd->events & POLLOUT;
      if (readBit && (accessMode == O_WRONLY || accessMode == O_RDWR)) {
        mask |= readBit;
      }
      auto writeBit = pollfd->events & POLLIN;
      if (writeBit && (accessMode == O_RDONLY || accessMode == O_RDWR)) {
        // If there is data in the file, then there is also the ability to read.
        // TODO: Does this need to consider the position as well? That is, if
        // the position is at the end, we can't read from the current position
        // at least. If we update this, make sure the size isn't an error!
        if (openFile->locked().getFile()->locked().getSize() > 0) {
          mask |= writeBit;
        }
      }
      // TODO: get mask from File dynamically using a poll() hook?
    }
    // TODO: set the state based on the state of the other end of the pipe, for
    //       pipes (POLLERR | POLLHUP)
    if (mask) {
      nonzero++;
    }
    pollfd->revents = mask;
  }
  // TODO: This should block based on the timeout. The old FS did not do so due
  //       to web limitations, which we should perhaps revisit (especially with
  //       pthreads and asyncify).
  return nonzero;
}

int __syscall_fallocate(int fd, int mode, off_t offset, off_t len) {
  assert(mode == 0); // TODO, but other modes were never supported in the old FS

  auto fileTable = wasmFS.getFileTable().locked();
  auto openFile = fileTable.getEntry(fd);
  if (!openFile) {
    return -EBADF;
  }

  auto dataFile = openFile->locked().getFile()->dynCast<DataFile>();
  // TODO: support for symlinks.
  if (!dataFile) {
    return -ENODEV;
  }

  auto locked = dataFile->locked();
  if (!(locked.getMode() & WASMFS_PERM_WRITE)) {
    return -EBADF;
  }

  if (offset < 0 || len <= 0) {
    return -EINVAL;
  }

  // TODO: We could only fill zeros for regions that were completely unused
  //       before, which for a backend with sparse data storage could make a
  //       difference. For that we'd need a new backend API.
  auto newNeededSize = offset + len;
  off_t size = locked.getSize();
  if (size < 0) {
    return size;
  }
  if (newNeededSize > size) {
    if (auto err = locked.setSize(newNeededSize)) {
      assert(err < 0);
      return err;
    }
  }

  return 0;
}

int __syscall_fcntl64(int fd, int cmd, ...) {
  auto fileTable = wasmFS.getFileTable().locked();
  auto openFile = fileTable.getEntry(fd);
  if (!openFile) {
    return -EBADF;
  }

  switch (cmd) {
    case F_DUPFD: {
      int newfd;
      va_list v1;
      va_start(v1, cmd);
      newfd = va_arg(v1, int);
      va_end(v1);
      if (newfd < 0) {
        return -EINVAL;
      }

      // Find the first available fd at arg or after.
      // TODO: Should we check for a limit on the max FD number, if we have one?
      while (1) {
        if (!fileTable.getEntry(newfd)) {
          (void)fileTable.setEntry(newfd, openFile);
          return newfd;
        }
        newfd++;
      }
    }
    case F_GETFD:
    case F_SETFD:
      // FD_CLOEXEC makes no sense for a single process.
      return 0;
    case F_GETFL:
      return openFile->locked().getFlags();
    case F_SETFL: {
      int flags;
      va_list v1;
      va_start(v1, cmd);
      flags = va_arg(v1, int);
      va_end(v1);
      // This syscall should ignore most flags.
      flags = flags & ~(O_RDONLY | O_WRONLY | O_RDWR | O_CREAT | O_EXCL |
                        O_NOCTTY | O_TRUNC);
      // Also ignore this flag which musl always adds constantly, but does not
      // matter for us.
      flags = flags & ~O_LARGEFILE;
      // On linux only a few flags can be modified, and we support only a subset
      // of those. Error on anything else.
      auto supportedFlags = flags & O_APPEND;
      if (flags != supportedFlags) {
        return -EINVAL;
      }
      openFile->locked().setFlags(flags);
      return 0;
    }
    case F_GETLK: {
      // If these constants differ then we'd need a case for both.
      static_assert(F_GETLK == F_GETLK64);
      flock* data;
      va_list v1;
      va_start(v1, cmd);
      data = va_arg(v1, flock*);
      va_end(v1);
      // We're always unlocked for now, until we implement byte-range locks.
      data->l_type = F_UNLCK;
      return 0;
    }
    case F_SETLK:
    case F_SETLKW: {
      static_assert(F_SETLK == F_SETLK64);
      static_assert(F_SETLKW == F_SETLKW64);
      // Always error for now, until we implement byte-range locks.
      return -EACCES;
    }
    default: {
      // TODO: support any remaining cmds
      return -EINVAL;
    }
  }
}

static int
doStatFS(boost::shared_ptr<File>& file, size_t size, struct statfs* buf) {
  if (size != sizeof(struct statfs)) {
    // We only know how to write to a standard statfs, not even a truncated one.
    return -EINVAL;
  }

  // NOTE: None of the constants here are true. We're just returning safe and
  //       sane values, that match the long-existing JS FS behavior (except for
  //       the inode number, where we can do better).
  buf->f_type = 0;
  buf->f_bsize = 4096;
  buf->f_frsize = 4096;
  buf->f_blocks = 1000000;
  buf->f_bfree = 500000;
  buf->f_bavail = 500000;
  buf->f_files = file->getIno();
  buf->f_ffree = 1000000;
  buf->f_fsid = {0, 0};
  buf->f_flags = ST_NOSUID;
  buf->f_namelen = 255;
  return 0;
}

int __syscall_statfs64(intptr_t path, size_t size, intptr_t buf) {
  auto parsed = path::parseFile((char*)path);
  if (auto err = parsed.getError()) {
    return err;
  }
  return doStatFS(parsed.getFile(), size, (struct statfs*)buf);
}

int __syscall_fstatfs64(int fd, size_t size, intptr_t buf) {
  auto openFile = wasmFS.getFileTable().locked().getEntry(fd);
  if (!openFile) {
    return -EBADF;
  }
  return doStatFS(openFile->locked().getFile(), size, (struct statfs*)buf);
}


int _mmap_js(size_t length,
             int prot,
             int flags,
             int fd,
             off_t offset,
             int* allocated,
             void** addr) {
  // PROT_EXEC is not supported (although we pretend to support the absence of
  // PROT_READ or PROT_WRITE).
  if ((prot & PROT_EXEC)) {
    return -EPERM;
  }

  if (!length) {
    return -EINVAL;
  }

  // One of MAP_PRIVATE, MAP_SHARED, or MAP_SHARED_VALIDATE must be used.
  int mapType = flags & MAP_TYPE;
  if (mapType != MAP_PRIVATE && mapType != MAP_SHARED &&
      mapType != MAP_SHARED_VALIDATE) {
    return -EINVAL;
  }

  if (mapType == MAP_SHARED_VALIDATE) {
    WASMFS_UNREACHABLE("TODO: MAP_SHARED_VALIDATE");
  }

  auto openFile = wasmFS.getFileTable().locked().getEntry(fd);
  if (!openFile) {
    return -EBADF;
  }

  boost::shared_ptr<DataFile> file;

  // Keep the open file info locked only for as long as we need that.
  {
    auto lockedOpenFile = openFile->locked();

    // Check permissions. We always need read permissions, since we need to read
    // the data in the file to map it.
    if ((lockedOpenFile.getFlags() & O_ACCMODE) == O_WRONLY) {
      return -EACCES;
    }

    // According to the POSIX spec it is possible to write to a file opened in
    // read-only mode with MAP_PRIVATE flag, as all modifications will be
    // visible only in the memory of the current process.
    if ((prot & PROT_WRITE) != 0 && mapType != MAP_PRIVATE &&
        (lockedOpenFile.getFlags() & O_ACCMODE) != O_RDWR) {
      return -EACCES;
    }

    file = lockedOpenFile.getFile()->dynCast<DataFile>();
  }

  if (!file) {
    return -ENODEV;
  }

  // TODO: On MAP_SHARED, install the mapping on the DataFile object itself so
  // that reads and writes can be redirected to the mapped region and so that
  // the mapping can correctly outlive the file being closed. This will require
  // changes to emscripten_mmap.c as well.

  // Align to a wasm page size, as we expect in the future to get wasm
  // primitives to do this work, and those would presumably be aligned to a page
  // size. Aligning now avoids confusion later.
  uint8_t* ptr = (uint8_t*)emscripten_builtin_memalign(WASM_PAGE_SIZE, length);
  if (!ptr) {
    return -ENOMEM;
  }

  auto nread = file->locked().read(ptr, length, offset);
  if (nread < 0) {
    // The read failed. Report the error, but first free the allocation.
    emscripten_builtin_free(ptr);
    return nread;
  }

  // From here on, we have succeeded, and can mark the allocation as having
  // occurred (which means that the caller has the responsibility to free it).
  *allocated = true;
  *addr = (void*)ptr;

  // The read must be of a valid amount, or we have had an internal logic error.
  assert(nread <= length);

  // mmap clears any extra bytes after the data itself.
  memset(ptr + nread, 0, length - nread);

  return 0;
}

int _msync_js(
  intptr_t addr, size_t length, int prot, int flags, int fd, off_t offset) {
  // TODO: This is not correct! Mappings should be associated with files, not
  // fds. Only need to sync if shared and writes are allowed.
  int mapType = flags & MAP_TYPE;
  if (mapType == MAP_SHARED && (prot & PROT_WRITE)) {
    __wasi_ciovec_t iovec;
    iovec.buf = (uint8_t*)addr;
    iovec.buf_len = length;
    __wasi_size_t nwritten;
    // Translate from WASI positive error codes to negative error codes.
    return -__wasi_fd_pwrite(fd, &iovec, 1, offset, &nwritten);
  }
  return 0;
}

int _munmap_js(
  intptr_t addr, size_t length, int prot, int flags, int fd, off_t offset) {
  // TODO: This is not correct! Mappings should be associated with files, not
  // fds.
  // TODO: Syncing should probably be handled in __syscall_munmap instead.
  return _msync_js(addr, length, prot, flags, fd, offset);
}

// Stubs (at least for now)

int __syscall_accept4(int sockfd,
                      intptr_t addr,
                      intptr_t addrlen,
                      int flags,
                      int dummy1,
                      int dummy2) {
  return -ENOSYS;
}

int __syscall_bind(
  int sockfd, intptr_t addr, size_t alen, int dummy, int dummy2, int dummy3) {
  return -ENOSYS;
}

int __syscall_connect(
  int sockfd, intptr_t addr, size_t len, int dummy, int dummy2, int dummy3) {
  return -ENOSYS;
}

int __syscall_socket(
  int domain, int type, int protocol, int dummy1, int dummy2, int dummy3) {
  return -ENOSYS;
}

int __syscall_listen(
  int sockfd, int backlock, int dummy1, int dummy2, int dummy3, int dummy4) {
  return -ENOSYS;
}

int __syscall_getsockopt(int sockfd,
                         int level,
                         int optname,
                         intptr_t optval,
                         intptr_t optlen,
                         int dummy) {
  return -ENOSYS;
}

int __syscall_getsockname(
  int sockfd, intptr_t addr, intptr_t len, int dummy, int dummy2, int dummy3) {
  return -ENOSYS;
}

int __syscall_getpeername(
  int sockfd, intptr_t addr, intptr_t len, int dummy, int dummy2, int dummy3) {
  return -ENOSYS;
}

int __syscall_sendto(
  int sockfd, intptr_t msg, size_t len, int flags, intptr_t addr, size_t alen) {
  return -ENOSYS;
}

int __syscall_sendmsg(
  int sockfd, intptr_t msg, int flags, intptr_t addr, size_t alen, int dummy) {
  return -ENOSYS;
}

int __syscall_recvfrom(int sockfd,
                       intptr_t msg,
                       size_t len,
                       int flags,
                       intptr_t addr,
                       intptr_t alen) {
  return -ENOSYS;
}

int __syscall_recvmsg(
  int sockfd, intptr_t msg, int flags, int dummy, int dummy2, int dummy3) {
  return -ENOSYS;
}

int __syscall_fadvise64(int fd, off_t offset, off_t length, int advice) {
  // Advice is currently ignored. TODO some backends might use it
  return 0;
}

int __syscall__newselect(int nfds,
                         intptr_t readfds_,
                         intptr_t writefds_,
                         intptr_t exceptfds_,
                         intptr_t timeout_) {
  // TODO: Implement this syscall. For now, we return an error code,
  //       specifically ENOMEM which is valid per the docs:
  //          ENOMEM Unable to allocate memory for internal tables
  //          https://man7.org/linux/man-pages/man2/select.2.html
  return -ENOMEM;
}
}
*/

namespace wasmfs {

FileTable::FileTable() {
/*  
  entries.emplace_back();
  (void)OpenFileState::create(
    SpecialFiles::getStdin(), O_RDONLY, entries.back());
  entries.emplace_back();
  (void)OpenFileState::create(
    SpecialFiles::getStdout(), O_WRONLY, entries.back());
  entries.emplace_back();
  (void)OpenFileState::create(
    SpecialFiles::getStderr(), O_WRONLY, entries.back());
  */  
}

boost::shared_ptr<OpenFileState> FileTable::Handle::getEntry(__wasi_fd_t fd) {
   /*
  if (fd >= fileTable.entries.size() || fd < 0) {
    return nullptr;
  }
  return fileTable.entries[fd];
  */
 return nullptr;
}

boost::shared_ptr<DataFile>
FileTable::Handle::setEntry(__wasi_fd_t fd,
                            boost::shared_ptr<OpenFileState> openFile) {
    /*
  assert(fd >= 0);
  if (fd >= fileTable.entries.size()) {
    fileTable.entries.resize(fd + 1);
  }
  if (openFile) {
    ++openFile->uses;
  }
  boost::shared_ptr<DataFile> ret;
  if (fileTable.entries[fd] && --fileTable.entries[fd]->uses == 0) {
    ret = fileTable.entries[fd]->locked().getFile()->dynCast<DataFile>();
  }
  fileTable.entries[fd] = openFile;
  return ret;
  */
 return nullptr;
}

__wasi_fd_t
FileTable::Handle::addEntry(boost::shared_ptr<OpenFileState> openFileState) {
  
/*
  // TODO: add freelist to avoid linear lookup time.
  for (__wasi_fd_t i = 0;; i++) {
    if (!getEntry(i)) {
      (void)setEntry(i, openFileState);
      return i;
    }
  }
  return -EBADF;
  */
}

int OpenFileState::create(boost::shared_ptr<File> file,
                          oflags_t flags,
                          boost::shared_ptr<OpenFileState>& out) {
    /*
  assert(file);
  std::vector<Directory::Entry> dirents;
  if (auto f = file->dynCast<DataFile>()) {
    if (int err = f->locked().open(flags & O_ACCMODE)) {
      return err;
    }
  } else if (auto d = file->dynCast<Directory>()) {
    // We are opening a directory; cache its contents for subsequent reads.
    auto lockedDir = d->locked();
    dirents = {{".", File::DirectoryKind, d->getIno()},
               {"..", File::DirectoryKind, lockedDir.getParent()->getIno()}};
    auto entries = lockedDir.getEntries();
    if (int err = entries.getError()) {
      return err;
    }
    dirents.insert(dirents.end(), entries->begin(), entries->end());
  }

  out = boost::make_shared<OpenFileState>(
    private_key{0}, flags, file, std::move(dirents));
  return 0;
  */
}
} // extern "C"


// Copyright 2021 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.
// This file defines the global state of the new file system.
// Current Status: Work in Progress.
// See https://github.com/emscripten-core/emscripten/issues/15041.



namespace wasmfs {

// Special backends that want to install themselves as the root use this hook.
// Otherwise, we use the default backends.
backend_t wasmfs_create_root_dir(void) {
/*
#ifdef WASMFS_CASE_INSENSITIVE
  //return createIgnoreCaseBackend([]() { return createMemoryBackend(); });
#else
  return createMemoryBackend();
#endif
*/
return NULL;
}

#ifdef WASMFS_CASE_INSENSITIVE
backend_t createIgnoreCaseBackend(std::function<backend_t()> createBacken);
#endif

// The below lines are included to make the compiler believe that the global
// constructor is part of a system header, which is necessary to work around a
// compilation error about using a reserved init priority less than 101. This
// ensures that the global state of the file system is constructed before
// anything else. ATTENTION: No other static global objects should be defined
// besides wasmFS. Due to # define _LIBCPP_INIT_PRIORITY_MAX
// __attribute__((init_priority(101))), we must use init priority 100 (reserved
// system priority) since wasmFS is a system level component.
// TODO: consider instead adding this in libc's startup code.
// WARNING: Maintain # n + 1 "wasmfs.cpp" 3 where n = line number.
# 29 "wasmfs.cpp" 3
__attribute__((init_priority(100))) WasmFS wasmFS;
# 31 "wasmfs.cpp"

// If the user does not implement this hook, do nothing.
__attribute__((weak)) extern "C" void wasmfs_before_preload(void) {}


// Set up global data structures and preload files.
WasmFS::WasmFS()  {
  
  rootDirectory = initRootDirectory();
  //cwd = rootDirectory;
  //auto rootBackend = wasmfs_create_root_dir();
  //wasmfs_before_preload();
  //preloadFiles();
  
}


// Manual integration with LSan. LSan installs itself during startup at the
// first allocation, which happens inside WasmFS code (since the WasmFS global
// object creates some data structures). As a result LSan's atexit() destructor
// will be called last, after WasmFS is cleaned up, since atexit() calls work
// are LIFO (like a stack). But that is a problem, since if WasmFS has shut
// down and deallocated itself then the leak code cannot actually print any of
// its findings, if it has any. To avoid that, define the LSan entry point as a
// weak symbol, and call it; if LSan is not enabled this can be optimized out,
// and if LSan is enabled then we'll check for leaks right at the start of the
// WasmFS destructor, which is the last time during which it is valid to print.
// (Note that this means we can't find leaks inside WasmFS code itself, but that
// seems fundamentally impossible for the above reasons, unless we made LSan log
// its findings in a way that does not depend on normal file I/O.)
__attribute__((weak)) extern "C" void __lsan_do_leak_check(void) {}

extern "C" void wasmfs_flush(void) {

  // Flush musl libc streams.
  fflush(0);

  // Flush our own streams. TODO: flush all backends.
  (void)SpecialFiles::getStdout()->locked().flush();
  (void)SpecialFiles::getStderr()->locked().flush();

}

WasmFS::~WasmFS() {
  // See comment above on this function.
  //
  // Note that it is ok for us to call it here, as LSan internally makes all
  // calls after the first into no-ops. That is, calling it here makes the one
  // time that leak checks are run be done here, or potentially earlier, but not
  // later; and as mentioned in the comment above, this is the latest possible
  // time for the checks to run (since right after this nothing can be `ed).
  //__lsan_do_leak_check();

  // TODO: Integrate musl exit() which would flush the libc part for us. That
  //       might also help with destructor priority - we need to happen last.
  //       (But we would still need to flush the internal WasmFS buffers, see
  //       wasmfs_flush() and the comment on it in the header.)
  //wasmfs_flush();

  // Break the reference cycle caused by the root directory being its own
  // parent.
  rootDirectory->locked();//.setParent(nullptr);
}




boost::shared_ptr<Directory> WasmFS::initRootDirectory() {
  auto rootBackend = wasmfs_create_root_dir();
  auto rootDirectory =
    rootBackend->createDirectory(S_IRUGO | S_IXUGO | S_IWUGO);
  auto lockedRoot = rootDirectory->locked();

  // The root directory is its own parent.
  lockedRoot.setParent(rootDirectory);

  // If the /dev/ directory does not already exist, create it. (It may already
  // exist in NODERAWFS mode, or if those files have been preloaded.)
  auto devDir = lockedRoot.insertDirectory("dev", S_IRUGO | S_IXUGO);

  if (devDir) {
    auto lockedDev = devDir->locked();
    lockedDev.mountChild("null", SpecialFiles::getNull());
    lockedDev.mountChild("stdin", SpecialFiles::getStdin());
    lockedDev.mountChild("stdout", SpecialFiles::getStdout());
    lockedDev.mountChild("stderr", SpecialFiles::getStderr());
    lockedDev.mountChild("random", SpecialFiles::getRandom());
    lockedDev.mountChild("urandom", SpecialFiles::getURandom());
  }
  

  // As with the /dev/ directory, it is not an error for /tmp/ to already
  // exist.
  lockedRoot.insertDirectory("tmp", S_IRWXUGO);

  return rootDirectory;
}

// Initialize files specified by the --preload-file option.
// Set up directories and files in wasmFS$preloadedDirs and
// wasmFS$preloadedFiles from JS. This function will be called before any file
// operation to ensure any preloaded files are eagerly available for use.

void WasmFS::preloadFiles() {
    return;
  // Debug builds only: add check to ensure preloadFiles() is called once.
#ifndef NDEBUG
  static std::atomic<int> timesCalled;
  timesCalled++;
  assert(timesCalled == 1);
#endif

  // Ensure that files are preloaded from the main thread.
  assert(emscripten_is_main_runtime_thread());

  auto numFiles = _wasmfs_get_num_preloaded_files();
  auto numDirs = _wasmfs_get_num_preloaded_dirs();

  // If there are no preloaded files, exit early.
  if (numDirs == 0 && numFiles == 0) {
    return;
  }

  // Iterate through wasmFS$preloadedDirs to obtain a parent and child pair.
  // Ex. Module['FS_createPath']("/foo/parent", "child", true, true);
  for (int i = 0; i < numDirs; i++) {
    char parentPath[PATH_MAX] = {};
    _wasmfs_get_preloaded_parent_path(i, parentPath);

    auto parsed = path::parseFile(parentPath);
    boost::shared_ptr<Directory> parentDir;
    if (parsed.getError() ||
        !(parentDir = parsed.getFile()->dynCast<Directory>())) {
      emscripten_err(
        "Fatal error during directory creation in file preloading.");
      abort();
    }

    char childName[PATH_MAX] = {};
    _wasmfs_get_preloaded_child_path(i, childName);

    auto lockedParentDir = parentDir->locked();
    if (lockedParentDir.getChild(childName)) {
      // The child already exists, so we don't need to do anything here.
      continue;
    }

    auto inserted =
      lockedParentDir.insertDirectory(childName, S_IRUGO | S_IXUGO);
    assert(inserted && "TODO: handle preload insertion errors");
  }

  for (int i = 0; i < numFiles; i++) {
    char fileName[PATH_MAX] = {};
    _wasmfs_get_preloaded_path_name(i, fileName);

    auto mode = _wasmfs_get_preloaded_file_mode(i);

    auto parsed = path::parseParent(fileName);
    if (parsed.getError()) {
      emscripten_err("Fatal error during file preloading");
      abort();
    }
    auto& [parent, childName] = parsed.getParentChild();
    auto created =
      parent->locked().insertDataFile(std::string(childName), (mode_t)mode);
    assert(created && "TODO: handle preload insertion errors");
    created->locked().preloadFromJS(i);
    
  }
}

} // namespace wasmfs

// Copyright 2021 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.
// This file defines the standard streams of the new file system.
// Current Status: Work in Progress.
// See https://github.com/emscripten-core/emscripten/issues/15041.


namespace wasmfs::SpecialFiles {

namespace {

// No-op reads and writes: /dev/null
class NullFile : public DataFile {
  int open(oflags_t) override { return 0; }
  int close() override { return 0; }

  ssize_t write(const uint8_t* buf, size_t len, off_t offset) override {
    return len;
  }

  ssize_t read(uint8_t* buf, size_t len, off_t offset) override { return 0; }

  int flush() override { return 0; }
  off_t getSize() override { return 0; }
  int setSize(off_t size) override { return -EPERM; }

public:
  NullFile() : DataFile(S_IRUGO | S_IWUGO, NullBackend, S_IFCHR) {}
};

class StdinFile : public DataFile {
  int open(oflags_t) override { return 0; }
  int close() override { return 0; }

  ssize_t write(const uint8_t* buf, size_t len, off_t offset) override {
    return -__WASI_ERRNO_INVAL;
  }

  ssize_t read(uint8_t* buf, size_t len, off_t offset) override {
    for (size_t i = 0; i < len; i++) {
      auto c = _wasmfs_stdin_get_char();
      if (c < 0) {
        // No more input can be read, return what we did read.
        return i;
      }
      buf[i] = c;
    }
    return len;
  };

  int flush() override { return 0; }
  off_t getSize() override { return 0; }
  int setSize(off_t size) override { return -EPERM; }

public:
  StdinFile() : DataFile(S_IRUGO, NullBackend, S_IFCHR) { seekable = false; }
};

// A standard stream that writes: stdout or stderr.
class WritingStdFile : public DataFile {
protected:
  std::vector<char> writeBuffer;

  int open(oflags_t) override { return 0; }
  int close() override { return 0; }

  ssize_t read(uint8_t* buf, size_t len, off_t offset) override {
    return -__WASI_ERRNO_INVAL;
  };

  int flush() override {
    // Write a null to flush the output if we have content.
    if (!writeBuffer.empty()) {
      const uint8_t nothing = '\0';
      write(&nothing, 1, 0);
    }
    return 0;
  }

  off_t getSize() override { return 0; }
  int setSize(off_t size) override { return -EPERM; }

  ssize_t writeToJS(const uint8_t* buf,
                    size_t len,
                    void (*console_write)(const char*),
                    std::vector<char>& fd_write_buffer) {
    for (size_t j = 0; j < len; j++) {
      uint8_t current = buf[j];
      // Flush on either a null or a newline.
      if (current == '\0' || current == '\n') {
        fd_write_buffer.push_back('\0'); // for null-terminated C strings
        console_write(fd_write_buffer.data());
        fd_write_buffer.clear();
      } else {
        fd_write_buffer.push_back(current);
      }
    }
    return len;
  }

public:
  WritingStdFile() : DataFile(S_IWUGO, NullBackend, S_IFCHR) {
    seekable = false;
  }
};

class StdoutFile : public WritingStdFile {
  ssize_t write(const uint8_t* buf, size_t len, off_t offset) override {
    // Node and worker threads issue in Emscripten:
    // https://github.com/emscripten-core/emscripten/issues/14804.
    // Issue filed in Node: https://github.com/nodejs/node/issues/40961
    // This is confirmed to occur when running with EXIT_RUNTIME and
    // PROXY_TO_PTHREAD. This results in only a single console.log statement
    // being outputted. The solution for now is to use out() and err() instead.
    //return writeToJS(buf, len, &emscripten_out, writeBuffer);
  }

public:
  StdoutFile() {}
};

class StderrFile : public WritingStdFile {
  ssize_t write(const uint8_t* buf, size_t len, off_t offset) override {
    // Similar issue with Node and worker threads as emscripten_out.
    // TODO: May not want to proxy stderr (fd == 2) to the main thread, as
    //       emscripten_err does.
    //       This will not show in HTML - a console.warn in a worker is
    //       sufficient. This would be a change from the current FS.
    //return writeToJS(buf, len, &emscripten_err, writeBuffer);
  }

public:
  StderrFile() {}
};

class RandomFile : public DataFile {
  int open(oflags_t) override { return 0; }
  int close() override { return 0; }

  ssize_t write(const uint8_t* buf, size_t len, off_t offset) override {
    return -__WASI_ERRNO_INVAL;
  }

  ssize_t read(uint8_t* buf, size_t len, off_t offset) override {
    uint8_t* end = buf + len;
    for (; buf < end; buf += 256) {
      [[maybe_unused]] int err = getentropy(buf, std::min(end - buf, 256l));
      assert(err == 0);
    }
    return len;
  };

  int flush() override { return 0; }
  off_t getSize() override { return 0; }
  int setSize(off_t size) override { return -EPERM; }

public:
  RandomFile() : DataFile(S_IRUGO, NullBackend, S_IFCHR) { seekable = false; }
};

} // anonymous namespace


boost::shared_ptr<DataFile> getNull() {
  static boost::shared_ptr<NullFile> null;
  return null;
}

boost::shared_ptr<DataFile> getStdin() {
  static boost::shared_ptr<StdinFile> stdin;
  return stdin;
}

boost::shared_ptr<DataFile> getStdout() {
  static boost::shared_ptr<StdoutFile> stdout;
  return stdout;
}

boost::shared_ptr<DataFile> getStderr() {
  static boost::shared_ptr<StderrFile> stderr;
  return stderr;
}

boost::shared_ptr<DataFile> getRandom() {
  static boost::shared_ptr<RandomFile> random;
  return random;
}

boost::shared_ptr<DataFile> getURandom() {
  static boost::shared_ptr<RandomFile> urandom;
  return urandom;
}


} // namespace wasmfs::SpecialFiles

// Copyright 2021 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.
// This file defines the file object of the new file system.
// Current Status: Work in Progress.
// See https://github.com/emscripten-core/emscripten/issues/15041.


namespace wasmfs {

//
// DataFile
//
void DataFile::Handle::preloadFromJS(int index) {
  // TODO: Each Datafile type could have its own impl of file preloading.
  // Create a buffer with the required file size.
  std::vector<uint8_t> buffer(_wasmfs_get_preloaded_file_size(index));

  // Ensure that files are preloaded from the main thread.
  assert(emscripten_is_main_runtime_thread());

  // Load data into the in-memory buffer.
  _wasmfs_copy_preloaded_file_data(index, buffer.data());

  write((const uint8_t*)buffer.data(), buffer.size(), 0);
}

//
// Directory
//

void Directory::Handle::cacheChild(const std::string& name,
                                   boost::shared_ptr<File> child,
                                   DCacheKind kind) {
  // Update the dcache if the backend hasn't opted out of using the dcache or if
  // this is a mount point, in which case it is not under the control of the
  // backend.
  if (kind == DCacheKind::Mount || !getDir()->maintainsFileIdentity()) {
    auto& dcache = getDir()->dcache;
    auto inserted = dcache.insert({name, {kind, child}});
    assert(inserted && "inserted child already existed!");
  }
    return;

  // Set the child's parent.
  assert(child->locked().getParent() == nullptr ||
         child->locked().getParent() == getDir());
  child->locked().setParent(getDir());
}

boost::shared_ptr<File> Directory::Handle::getChild(const std::string& name) {
  // Unlinked directories must be empty, without even "." or ".."
  if (!getParent()) {
    return nullptr;
  }
  if (name == ".") {
    return file;
  }
  if (name == "..") {
    return getParent();
  }
  // Check whether the cache already contains this child.
  auto& dcache = getDir()->dcache;
  if (auto it = dcache.find(name); it != dcache.end()) {
    return it->second.file;
  }
  // Otherwise check whether the backend contains an underlying file we don't
  // know about.
  auto child = getDir()->getChild(name);
  if (!child) {
    return nullptr;
  }
  cacheChild(name, child, DCacheKind::Normal);
  return child;
}

bool Directory::Handle::mountChild(const std::string& name,
                                   boost::shared_ptr<File> child) {
  assert(child);
  // Cannot insert into an unlinked directory.
  if (!getParent()) {
    return false;
  }
  cacheChild(name, child, DCacheKind::Mount);
  return true;
}

boost::shared_ptr<DataFile>
Directory::Handle::insertDataFile(const std::string& name, mode_t mode) {
  // Cannot insert into an unlinked directory.
  if (!getParent()) {
    return nullptr;
  }
  auto child = getDir()->insertDataFile(name, mode);
  if (!child) {
    return nullptr;
  }
  cacheChild(name, child, DCacheKind::Normal);
  updateMTime();
  return child;
}

boost::shared_ptr<Directory>
Directory::Handle::insertDirectory(const std::string& name, mode_t mode) {
  // Cannot insert into an unlinked directory.
  if (!getParent()) {
    return nullptr;
  }
  auto child = getDir()->insertDirectory(name, mode);
  if (!child) {
    return nullptr;
  }
  cacheChild(name, child, DCacheKind::Normal);
  return nullptr;
  updateMTime();
  return child;
}


boost::shared_ptr<Symlink>
Directory::Handle::insertSymlink(const std::string& name,
                                 const std::string& target) {
  // Cannot insert into an unlinked directory.
  if (!getParent()) {
    return nullptr;
  }
  auto child = getDir()->insertSymlink(name, target);
  if (!child) {
    return nullptr;
  }
  cacheChild(name, child, DCacheKind::Normal);
  updateMTime();
  return child;
}

// TODO: consider moving this to be `Backend::move` to avoid asymmetry between
// the source and destination directories and/or taking `Directory::Handle`
// arguments to prove that the directories have already been locked.
int Directory::Handle::insertMove(const std::string& name,
                                  boost::shared_ptr<File> file) {
  // Cannot insert into an unlinked directory.
  if (!getParent()) {
    return -EPERM;
  }

  // Look up the file in its old parent's cache.
  auto oldParent = file->locked().getParent();
  auto& oldCache = oldParent->dcache;
  auto oldIt = std::find_if(oldCache.begin(), oldCache.end(), [&](auto& kv) {
    return kv.second.file == file;
  });

  // TODO: Handle moving mount points correctly by only updating caches without
  // involving the backend.

  // Attempt the move.
  if (auto err = getDir()->insertMove(name, file)) {
    return err;
  }

  if (oldIt != oldCache.end()) {
    // Do the move and update the caches.
    auto [oldName, entry] = *oldIt;
    assert(oldName.size());
    // Update parent pointers and caches to reflect the successful move.
    oldCache.erase(oldIt);
    auto& newCache = getDir()->dcache;
    auto [it, inserted] = newCache.insert({name, entry});
    if (!inserted) {
      // Update and overwrite the overwritten file.
      it->second.file->locked().setParent(nullptr);
      it->second = entry;
    }
  } else {
    // This backend doesn't use the dcache.
    assert(getDir()->maintainsFileIdentity());
  }

  file->locked().setParent(getDir());

  // TODO: Moving mount points probably shouldn't update the mtime.
  oldParent->locked().updateMTime();
  updateMTime();

  return 0;
}

int Directory::Handle::removeChild(const std::string& name) {
  auto& dcache = getDir()->dcache;
  auto entry = dcache.find(name);
  // If this is a mount, we don't need to call into the backend.
  if (entry != dcache.end() && entry->second.kind == DCacheKind::Mount) {
    dcache.erase(entry);
    return 0;
  }
  if (auto err = getDir()->removeChild(name)) {
    assert(err < 0);
    return err;
  }
  if (entry != dcache.end()) {
    entry->second.file->locked().setParent(nullptr);
    dcache.erase(entry);
  }
  updateMTime();
  return 0;
}

std::string Directory::Handle::getName(boost::shared_ptr<File> file) {
  if (getDir()->maintainsFileIdentity()) {
    return getDir()->getName(file);
  }
  auto& dcache = getDir()->dcache;
  for (auto it = dcache.begin(); it != dcache.end(); ++it) {
    if (it->second.file == file) {
      return it->first;
    }
  }
  return "";
}

ssize_t Directory::Handle::getNumEntries() {
  size_t mounts = 0;
  auto& dcache = getDir()->dcache;
  for (auto it = dcache.begin(); it != dcache.end(); ++it) {
    if (it->second.kind == DCacheKind::Mount) {
      ++mounts;
    }
  }
  auto numReal = getDir()->getNumEntries();
  if (numReal < 0) {
    return numReal;
  }
  return numReal + mounts;
}

Directory::MaybeEntries Directory::Handle::getEntries() {
  auto entries = getDir()->getEntries();
  if (entries.getError()) {
    return entries;
  }
  auto& dcache = getDir()->dcache;
  for (auto it = dcache.begin(); it != dcache.end(); ++it) {
    auto& [name, entry] = *it;
    if (entry.kind == DCacheKind::Mount) {
      entries->push_back({name, entry.file->kind, entry.file->getIno()});
    }
  }
  return entries;
}
} // namespace wasmfs

// Copyright 2022 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

namespace wasmfs::path {

namespace {

static inline constexpr size_t MAX_RECURSIONS = 40;

ParsedFile doParseFile(std::string_view path,
                       boost::shared_ptr<Directory> base,
                       LinkBehavior links,
                       size_t& recursions);

ParsedFile getBaseDir(__wasi_fd_t basefd) {
  if (basefd == AT_FDCWD) {
    return {wasmFS.getCWD()};
  }
  auto openFile = wasmFS.getFileTable().locked().getEntry(basefd);
  if (!openFile) {
    return -EBADF;
  }
  if (auto baseDir = openFile->locked().getFile()->dynCast<Directory>()) {
    return {baseDir};
  }
  return -ENOTDIR;
}

ParsedFile getChild(boost::shared_ptr<Directory> dir,
                    std::string_view name,
                    LinkBehavior links,
                    size_t& recursions) {
  auto child = dir->locked().getChild(std::string(name));
  if (!child) {
    return -ENOENT;
  }
  if (links != NoFollowLinks) {
    while (auto link = child->dynCast<Symlink>()) {
      if (++recursions > MAX_RECURSIONS) {
        return -ELOOP;
      }
      auto target = link->getTarget();
      if (target.empty()) {
        return -ENOENT;
      }
      auto parsed = doParseFile(target, dir, FollowLinks, recursions);
      if (auto err = parsed.getError()) {
        return err;
      }
      child = parsed.getFile();
    }
  }
  return child;
}
ParsedParent doParseParent(std::string_view path,
                           boost::shared_ptr<Directory> curr,
                           size_t& recursions) {
  // Empty paths never exist.
  if (path.empty()) {
    return {-ENOENT};
  }

  // Handle absolute paths.
  if (path.front() == '/') {
    curr = wasmFS.getRootDirectory();
    path.remove_prefix(1);
  }

  // Ignore trailing '/'.
  while (!path.empty() && path.back() == '/') {
    path.remove_suffix(1);
  }

  // An empty path here means that the path was equivalent to "/" and does not
  // contain a child segment for us to return. The root is its own parent, so we
  // can handle this by returning (root, ".").
  if (path.empty()) {
    return {std::make_pair(std::move(curr), std::string_view("."))};
  }

  while (true) {
    // Skip any leading '/' for each segment.
    while (!path.empty() && path.front() == '/') {
      path.remove_prefix(1);
    }

    // If this is the leaf segment, return.
    size_t segment_end = path.find_first_of('/');
    if (segment_end == std::string_view::npos) {
      return {std::make_pair(std::move(curr), path)};
    }

    // Try to descend into the child segment.
    // TODO: Check permissions on intermediate directories.
    auto segment = path.substr(0, segment_end);
    auto child = getChild(curr, segment, FollowLinks, recursions);
    if (auto err = child.getError()) {
      return err;
    }
    curr = child.getFile()->dynCast<Directory>();
    if (!curr) {
      return -ENOTDIR;
    }
    path.remove_prefix(segment_end);
  }
}

ParsedFile doParseFile(std::string_view path,
                       boost::shared_ptr<Directory> base,
                       LinkBehavior links,
                       size_t& recursions) {
  auto parsed = doParseParent(path, base, recursions);
  if (auto err = parsed.getError()) {
    return {err};
  }
  auto& [parent, child] = parsed.getParentChild();
  return getChild(parent, child, links, recursions);
}
} // anonymous namespace

ParsedParent parseParent(std::string_view path, __wasi_fd_t basefd) {
  auto base = getBaseDir(basefd);
  if (auto err = base.getError()) {
    return err;
  }
  size_t recursions = 0;
  auto baseDir = base.getFile()->cast<Directory>();
  return doParseParent(path, baseDir, recursions);
}

ParsedFile parseFile(std::string_view path, __wasi_fd_t basefd, LinkBehavior links) {
    
  auto base = getBaseDir(basefd);
  if (auto err = base.getError()) {
    return err;
  }
  size_t recursions = 0;
  auto baseDir = base.getFile()->cast<Directory>();
  return doParseFile(path, baseDir, links, recursions);
  
}

ParsedFile getFileAt(__wasi_fd_t fd, std::string_view path, int flags) {
    
  if ((flags & AT_EMPTY_PATH) && path.size() == 0) {
    // Don't parse a path, just use `dirfd` directly.
    if (fd == AT_FDCWD) {
      return {wasmFS.getCWD()};
    }
    auto openFile = wasmFS.getFileTable().locked().getEntry(fd);
    if (!openFile) {
      return {-EBADF};
    }
    return {openFile->locked().getFile()};
  }
  auto links = (flags & AT_SYMLINK_NOFOLLOW) ? NoFollowLinks : FollowLinks;
  return path::parseFile(path, fd, links);
  
}

ParsedFile getFileFrom(boost::shared_ptr<Directory> base, std::string_view path) {
    
  size_t recursions = 0;
  return doParseFile(path, base, FollowLinks, recursions);
  
}

} // namespace wasmfs::path

// Copyright 2021 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

// This file defines the memory file backend of the new file system.
// Current Status: Work in Progress.
// See https://github.com/emscripten-core/emscripten/issues/15041.


namespace wasmfs {

ssize_t MemoryDataFile::write(const uint8_t* buf, size_t len, off_t offset) {
  if (offset + len > buffer.size()) {
    if (offset + len > buffer.max_size()) {
      // Overflow: the necessary size fits in an off_t, but cannot fit in the
      // container.
      return -EIO;
    }
    buffer.resize(offset + len);
  }
  std::memcpy(&buffer[offset], buf, len);
  return len;
}

ssize_t MemoryDataFile::read(uint8_t* buf, size_t len, off_t offset) {
  if (offset >= buffer.size()) {
    len = 0;
  } else if (offset + len >= buffer.size()) {
    len = buffer.size() - offset;
  }
  std::memcpy(buf, &buffer[offset], len);
  return len;
}

std::vector<MemoryDirectory::ChildEntry>::iterator
MemoryDirectory::findEntry(const std::string& name) {
  return std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
    return entry.name == name;
  });
}

boost::shared_ptr<File> MemoryDirectory::getChild(const std::string& name) {
  if (auto entry = findEntry(name); entry != entries.end()) {
    return entry->child;
  }
  return nullptr;
}

int MemoryDirectory::removeChild(const std::string& name) {
  auto entry = findEntry(name);
  if (entry != entries.end()) {
    entry->child->locked().setParent(nullptr);
    entries.erase(entry);
  }
  return 0;
}

Directory::MaybeEntries MemoryDirectory::getEntries() {
  std::vector<Directory::Entry> result;
  result.reserve(entries.size());
  for (auto& [name, child] : entries) {
    result.push_back({name, child->kind, child->getIno()});
  }
  return {result};
}

int MemoryDirectory::insertMove(const std::string& name,
                                boost::shared_ptr<File> file) {
  auto& oldEntries =
    boost::static_pointer_cast<MemoryDirectory>(file->locked().getParent())
      ->entries;
  for (auto it = oldEntries.begin(); it != oldEntries.end(); ++it) {
    if (it->child == file) {
      oldEntries.erase(it);
      break;
    }
  }
  (void)removeChild(name);
  insertChild(name, file);
  return 0;
}

std::string MemoryDirectory::getName(boost::shared_ptr<File> file) {
  auto it =
    std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
      return entry.child == file;
    });
  if (it != entries.end()) {
    return it->name;
  }
  return "";
}

class MemoryBackend : public Backend {
public:
  boost::shared_ptr<DataFile> createFile(mode_t mode) override {
    return boost::make_shared<MemoryDataFile>(mode, this);
  }
  boost::shared_ptr<Directory> createDirectory(mode_t mode) override {
    return boost::make_shared<MemoryDirectory>(mode, this);
  }
  boost::shared_ptr<Symlink> createSymlink(std::string target) override {
    return boost::make_shared<MemorySymlink>(target, this);
  }
};

backend_t createMemoryBackend() {
  return wasmFS.addBackend(std::make_unique<MemoryBackend>());
}

extern "C" {

backend_t wasmfs_create_memory_backend() { return createMemoryBackend(); }

} // extern "C"

} // namespace wasmfs

// Copyright 2022 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.



namespace wasmfs {

void handle_unreachable(const char* msg, const char* file, unsigned line) {
#ifndef NDEBUG
#ifdef __EMSCRIPTEN__
  if (msg) {
    emscripten_err(msg);
  }
  emscripten_err("UNREACHABLE executed");
  if (file) {
    emscripten_errf("at %s:%d", file, line);
  }
#else // EMSCRIPTEN
  if (msg) {
    std::cerr << msg << "\n";
  }
  std::cerr << "UNREACHABLE executed";
  if (file) {
    std::cerr << " at " << file << ":" << line;
  }
  std::cerr << "!\n";
#endif
  // TODO: sanitizer integration, see binaryen's similar code
#endif
  __builtin_trap();
}

} // namespace wasmfs
