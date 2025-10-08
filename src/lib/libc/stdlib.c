#include "stdlib.h"
#include "ctype.h"

int atoi(const char *nptr) {
  if (!nptr) {
    return 0;
  }

  const char *s = nptr;
  while (isspace((unsigned char)*s)) {
    s++;
  }

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
