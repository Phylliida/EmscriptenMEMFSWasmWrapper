#include <emscripten.h>
#include <stdio.h>

EMSCRIPTEN_KEEPALIVE
void init_memfs() {
    printf("MEMFS initialized\n");
}
