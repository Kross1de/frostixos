#ifndef CTYPE_H
#define CTYPE_H

static inline int isdigit(int c)
{
	return (c >= '0' && c <= '9');
}
static inline int isspace(int c)
{
	return (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t'
		|| c == '\v');
}

static inline int isalpha(int c)
{
	return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

static inline int toupper(int c)
{
	if (c >= 'a' && c <= 'z') {
		return c - 'a' + 'A';
	}
	return c;
}

#endif
