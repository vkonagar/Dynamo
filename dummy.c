#include<stdio.h>
#include<dlfcn.h>


int main()
{
    void* handle;
    void (*execute)(int fd, char* args[], int count);
    char* error;

    /* Dynamically load the library */
    handle = dlopen("./hey.so", RTLD_LAZY);
    if (!handle)
    {
        fprintf(stderr, "%s\n", dlerror());
    }

    void * p = dlsym(handle, "cgi_function");

    if (dlclose(handle) < 0)
    {
        fprintf(stderr, "%s\n", dlerror());
        return -1;
    }
}
