# Makefile for MEMFS WASM compilation with filesystem operations

# Compiler
EMCC = emcc

# Source file
SRC = memfs_init.c

# Output file
OUT = memfs.wasm

# Emscripten flags
EMFLAGS = -s WASM=1 \
          -s WASMFS=1 \
          -s SIDE_MODULE=0 \
          -s EXPORTED_FUNCTIONS='["_bees"]' \
          -s FILESYSTEM=1 \
          -s NO_EXIT_RUNTIME=1 \
          -s FORCE_FILESYSTEM=1 \
          -s EXPORTED_RUNTIME_METHODS='["FS"]' \
          -s STANDALONE_WASM \
          -s ALLOW_MEMORY_GROWTH=1 \
          -v \
          -O3 \
          --no-entry

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
