#ifndef LIBSTR_H
#define LIBSTR_H

int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, unsigned int n);
unsigned int strlen(const char *s);
void strcpy(char *dest, const char *src);

#endif