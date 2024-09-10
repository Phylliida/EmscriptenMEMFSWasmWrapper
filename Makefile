# Makefile for MEMFS WASM compilation with filesystem operations

# Compiler
EMCC = ./emscripten/emcc

# Source file
SRC = ayy.cpp 
# Output file
OUT = memfs.wasm

# Emscripten flags
EMFLAGS = -s WASM=1 \
          -s WASMFS=0 \
          -s SIDE_MODULE=0 \
          -s EXPORTED_FUNCTIONS='[]' \
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
