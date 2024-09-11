# Makefile for MEMFS WASM compilation with filesystem operations

# Compiler
EMCC = ./emscripten/emcc

# Source file
SRC = memfs.cpp
# Output file
OUT = memfs.wasm

# we are limited in memory size because pointer is 32 bit, could use -sMEMORY64=1 but that results in functions that don't match standard usage
# Emscripten flags
# need WASMFS=0 because we had to modify some of the files
EMFLAGS = -s WASM=1 \
          -s WASMFS=0 \
          -s SIDE_MODULE=0 \
          -s EXPORTED_FUNCTIONS="['_malloc', '_free']" \
          -s FILESYSTEM=1 \
          -s FORCE_FILESYSTEM=1 \
          -s EXPORTED_RUNTIME_METHODS='["FS"]' \
          -s STANDALONE_WASM \
          -s ALLOW_MEMORY_GROWTH=1 \
          -s USE_BOOST_HEADERS=1 \
          -Iwasmfs \
          -fno-exceptions \
          --no-entry

          # this will import unneded stuff -fwasm-exceptions \

# Default target
all: $(OUT)

# Compile MEMFS to WASM
$(OUT): $(SRC)
	$(EMCC) $(SRC) $(EMFLAGS) -o $(OUT)

# Clean build artifacts
clean:
	rm -f $(OUT)

# Phony targets
.PHONY: all clean
