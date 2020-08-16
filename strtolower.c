#include "strtolower.h"

char* strtolower(char* input) {
    int i, s = strlen(input);
    char* lower = malloc(sizeof(char) * (s + 1));
    for (i = 0; i < s; i++) {
        lower[i] = tolower(input[i]);
    }
    lower[s] = 0;
    return lower;
}
