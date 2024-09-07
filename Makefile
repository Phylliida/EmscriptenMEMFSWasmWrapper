

# Makefile for MEMFS WASM compilation

# Compiler
EMCC = emcc

# Source file
SRC = memfs_init.c

# Output file
OUT = memfs.wasm

# Emscripten flags
EMFLAGS = -s WASM=1 \
          -s SIDE_MODULE=2 \
          -s EXPORTED_RUNTIME_METHODS='["FS"]' \
          -s FORCE_FILESYSTEM=1 \
          -s EXPORTED_FUNCTIONS='["_init_memfs"]' \
          -s ENVIRONMENT='web,worker' \
          -s STANDALONE_WASM \
          -s ERROR_ON_UNDEFINED_SYMBOLS=0

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
