#include <stdio.h>

// Function to print the current function's name
void __attribute__((weak)) __print_func_name(const char *name, const char* file_name, int line_num, int col_num) {
    fprintf(stderr, "Entering function: %s Location: %s:%d:%d\n", name, file_name, line_num, col_num);
    fflush(stderr);
}