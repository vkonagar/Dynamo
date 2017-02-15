#include <stdio.h>
#include "util.h"
struct dynlib_cache
{
    char file_name[MAX_DLL_NAME_LENGTH];
    int size;
    struct dynlib_cache* next;
};

void* load_dyn_library(char* library_name);
int unload_dyn_library(void* lib_handle);
