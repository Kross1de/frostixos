#include <kernel/kernel.h>

size_t strlen(const char* str) {
    if (!str) {
        return 0;
    }
    
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    
    return len;
}

int strcmp(const char* s1, const char* s2) {
    if (!s1 || !s2) {
        return s1 == s2 ? 0 : (s1 ? 1 : -1);
    }
    
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    if (!s1 || !s2 || n == 0) {
        return 0;
    }
    
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    
    if (n == 0) {
        return 0;
    }
    
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

char* strcpy(char* dest, const char* src) {
    if (!dest || !src) {
        return dest;
    }
    
    char* original_dest = dest;
    
    while ((*dest++ = *src++)) {
        ////////////////////////
    }
    
    return original_dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    if (!dest || !src || n == 0) {
        return dest;
    }
    
    char* original_dest = dest;
    
    while (n > 0 && *src) {
        *dest++ = *src++;
        n--;
    }
    
    while (n > 0) {
        *dest++ = '\0';
        n--;
    }
    
    return original_dest;
}

void* memset(void* ptr, int value, size_t num) {
    if (!ptr || num == 0) {
        return ptr;
    }
    
    unsigned char* byte_ptr = (unsigned char*)ptr;
    unsigned char byte_value = (unsigned char)value;
    
    for (size_t i = 0; i < num; i++) {
        byte_ptr[i] = byte_value;
    }
    
    return ptr;
}

void* memcpy(void* dest, const void* src, size_t num) {
    if (!dest || !src || num == 0) {
        return dest;
    }
    
    unsigned char* dest_byte = (unsigned char*)dest;
    const unsigned char* src_byte = (const unsigned char*)src;
    
    for (size_t i = 0; i < num; i++) {
        dest_byte[i] = src_byte[i];
    }
    
    return dest;
}

void* memmove(void* dest, const void* src, size_t num) {
    if (!dest || !src || num == 0) {
        return dest;
    }

    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;

    if (d < s) {
        for (size_t i = 0; i < num; i++) {
            d[i] = s[i];
        }
    } else if (d > s) {
        for (size_t i = num; i != 0; i--) {
            d[i-1] = s[i-1];
        }
    }

    return dest;
}

int memcmp(const void* ptr1, const void* ptr2, size_t num) {
    if (!ptr1 || !ptr2 || num == 0) {
        return 0;
    }
    
    const unsigned char* byte1 = (const unsigned char*)ptr1;
    const unsigned char* byte2 = (const unsigned char*)ptr2;
    
    for (size_t i = 0; i < num; i++) {
        if (byte1[i] != byte2[i]) {
            return byte1[i] - byte2[i];
        }
    }
    
    return 0;
}
