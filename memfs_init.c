#include <emscripten.h>
#include <stdio.h>
#include <string.h>

EMSCRIPTEN_KEEPALIVE
void bees() {
   FILE* file = fopen("bees", "w");
   const char* text = "hello";
   fwrite(text, sizeof(char), strlen(text), file);
   fclose(file);
}

