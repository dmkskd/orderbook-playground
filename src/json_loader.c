// json_loader.c
#include "../include/json_loader.h"
#include <stdio.h>
#include <stdlib.h>

char* load_json_file(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    size_t bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != file_size) {
        free(buffer);
        return NULL;
    }
    
    buffer[file_size] = '\0';
    return buffer;
}

void free_json_data(char* data) {
    free(data);
}
