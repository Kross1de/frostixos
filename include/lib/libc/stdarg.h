#ifndef STDARG_H
#define STDARG_H

typedef char* va_list;

#define va_start(ap, last) ((ap) = (va_list)&(last) + sizeof(last))
#define va_arg(ap, type) (*(type*)((ap) += sizeof(type), (ap) - sizeof(type)))
#define va_end(ap) ((ap) = (va_list)0)
#define va_copy(dest, src) ((dest) = (src))

#endif // STDARG_H
