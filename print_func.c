#include <stdio.h>

// Function to print the current function's name
void __attribute__((weak)) __print_func_name(const char *name) {
    fprintf(stderr, "Entering function: %s\n", name);
    fflush(stderr);
}