#include <stdio.h>

#define MAX_DLL_NAME_LENGTH 10
#define MAX_DLL_PATH_LENGTH 10

struct dynlib_cache
{
    char file_name[MAX_DLL_NAME_LENGTH];
    int size;
    struct dynlib_cache* next;
};

void* load_dyn_library(char* library_name);
int unload_dyn_library(void* lib_handle);
