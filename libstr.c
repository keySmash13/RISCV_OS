#include "libstr.h"

// Compare full strings (like strcmp)
int strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) a++, b++;
    return (unsigned char)*a - (unsigned char)*b;
}

// Compare up to n characters (like strncmp)
int strncmp(const char *a, const char *b, unsigned int n) {
    unsigned int i;
    for (i = 0; i < n; i++) {
        if (a[i] != b[i] || a[i] == '\0' || b[i] == '\0')
            return (unsigned char)a[i] - (unsigned char)b[i];
    }
    return 0;
}

// Minimal strlen implementation
unsigned int strlen(const char *s) {
    unsigned int len = 0;
    while (s[len]) len++;       // Count chars until null terminator
    return len;
}

// Minimal strcpy implementation
void strcpy(char *dest, const char *src) {
    while ((*dest++ = *src++)) ; // Copy including '\0'
}