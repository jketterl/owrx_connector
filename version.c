#include "version.h"
#include <stdio.h>
#include <unistd.h>

void print_version() {
    fprintf(stdout, "owrx-connector version %s\n", VERSION);
}