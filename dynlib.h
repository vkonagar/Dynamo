#ifndef __DYNLIB_H
#define __DYBLIB_H

#include <stdio.h>
#include "util.h"

void* load_dyn_library(char* library_name);
int unload_dyn_library(void* lib_handle);

#endif /* __DYBLIB_H */
