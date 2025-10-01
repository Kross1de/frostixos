#include "ctype.h"
#include "stdlib.h"

int atoi(const char *nptr) {
  if (!nptr) {
    return 0;
  }

  // Skip leading whitespace
  const char *s = nptr;
  while (isspace((unsigned char)*s)) {
    s++;
  }

  // Handle optional sign
  int sign = 1;
  if (*s == '+') {
    s++;
  } else if (*s == '-') {
    sign = -1;
    s++;
  }

  int result = 0;
  while (isdigit((unsigned char)*s)) {
    result = result * 10 + (*s - '0');
    s++;
  }
  return sign * result;
}