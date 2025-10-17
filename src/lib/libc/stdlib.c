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

u32 hex_to_u32(const char *str) {
  u32 val = 0;
  if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
    str += 2;
  }
while (*str) {
    char c = *str++;
    if (c >= '0' && c <= '9') {
      val = (val << 4) | (c - '0');
    } else if (c >= 'a' && c <= 'f') {
      val = (val << 4) | (c - 'a' + 10);
    } else if (c >= 'A' && c <= 'F') {
      val = (val << 4) | (c - 'A' + 10);
    } else {
      break;
    }
  }
  return val;
}
