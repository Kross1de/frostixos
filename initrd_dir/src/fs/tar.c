#include <fs/tar.h>
#include <lib/libc/stdio.h>
#include <string.h>
#include <printf.h>
#include <stdlib.h>

/* Round up to 512-byte blocks */
static inline size_t round_up_512(size_t x)
{
    return (x + (TAR_BLOCK_SIZE - 1)) & ~(TAR_BLOCK_SIZE - 1);
}

/* Parse octal string fields (size, mtime, etc). The field may be NUL-or
 * space-padded. Returns 0 on error/empty.
 */
static unsigned long tar_parse_octal(const char *str, size_t len)
{
    char buf[32];
    size_t n = (len < sizeof(buf) - 1) ? len : (sizeof(buf) - 1);
    size_t i = 0;
    for (; i < n; i++) {
        char c = str[i];
        if (c == '\0' || c == ' ')
            break;
        buf[i] = c;
    }
    buf[i] = '\0';
    if (i == 0)
        return 0;
    return strtoul(buf, NULL, 8);
}

static int header_is_zero(const tar_header_t *h)
{
    const unsigned char *p = (const unsigned char *)h;
    for (size_t i = 0; i < TAR_BLOCK_SIZE; i++)
        if (p[i] != 0)
            return 0;
    return 1;
}

bool tar_validate_header(const tar_header_t *hdr)
{
    if (!hdr)
        return false;
    /* checksum: sum of header bytes with chksum field treated as spaces */
    const unsigned char *p = (const unsigned char *)hdr;
    unsigned long sum = 0;
    for (size_t i = 0; i < TAR_BLOCK_SIZE; i++)
        sum += p[i];
    /* subtract the actual chksum field bytes and add spaces */
    for (size_t i = 0; i < 8; i++)
        sum -= (unsigned char)hdr->chksum[i];
    sum += ' ' * 8;

    unsigned long chk = tar_parse_octal(hdr->chksum, sizeof(hdr->chksum));
    if (chk != sum)
        return false;

    /* if magic set, should be "ustar" */
    if (hdr->magic[0]) {
        if (strncmp(hdr->magic, "ustar", 5) != 0)
            return false;
    }
    return true;
}

static void build_full_name(const tar_header_t *hdr, char *out, size_t out_sz)
{
    out[0] = '\0';
    if (hdr->prefix[0]) {
        strncpy(out, hdr->prefix, out_sz - 1);
        out[out_sz - 1] = '\0';
        size_t l = strlen(out);
        if (l + 1 < out_sz) {
            out[l++] = '/';
            out[l] = '\0';
        }
        /* append name */
        size_t i = 0;
        while (i < out_sz - 1 - l && hdr->name[i]) {
            out[l + i] = hdr->name[i];
            i++;
        }
        out[l + i] = '\0';
    } else {
        strncpy(out, hdr->name, out_sz - 1);
        out[out_sz - 1] = '\0';
    }
}

/* Max buffer for long file names from GNU tar 'L' entries. This is intentionally
 * bounded to avoid unbounded stack or heap usage when parsing malformed archives.
 */
#define TAR_LONGNAME_MAX 4096

void tar_list(const void *archive, size_t archive_size)
{
    const unsigned char *base = (const unsigned char *)archive;
    const unsigned char *end = base + archive_size;

    const unsigned char *p = base;
    char long_name[TAR_LONGNAME_MAX];
    long_name[0] = '\0';

    while (p + TAR_BLOCK_SIZE <= end) {
        const tar_header_t *hdr = (const tar_header_t *)p;
        if (header_is_zero(hdr))
            break; /* end marker */

        unsigned long fsize = tar_parse_octal(hdr->size, sizeof(hdr->size));

        /* Handle GNU longname entry: typeflag 'L' contains the filename in
         * the following data blocks; the next header describes the actual file. */
        if (hdr->typeflag == 'L') {
            /* read the long name into buffer (truncate if too long) */
            size_t copy_len = (fsize < (TAR_LONGNAME_MAX - 1)) ? (size_t)fsize : (TAR_LONGNAME_MAX - 1);
            const unsigned char *data = p + TAR_BLOCK_SIZE;
            if (data + copy_len > end)
                break; /* truncated */
            memcpy(long_name, data, copy_len);
            long_name[copy_len] = '\0';
            /* advance past header + data */
            size_t skip = TAR_BLOCK_SIZE + round_up_512((size_t)fsize);
            if (p + skip > end)
                break;
            p += skip;
            /* continue to next header which is the real file entry */
            continue;
        }

        char name[256];
        if (long_name[0]) {
            /* use long_name (truncate if necessary) */
            strncpy(name, long_name, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
            long_name[0] = '\0'; /* consumed */
        } else {
            build_full_name(hdr, name, sizeof(name));
        }

        const char *kind = (hdr->typeflag == '5') ? "dir" : "file";
        printf("%7s %8lu  %s\n", kind, fsize, name);

        size_t skip = TAR_BLOCK_SIZE + round_up_512((size_t)fsize);
        if (p + skip > end)
            break; /* truncated */
        p += skip;
    }
}

const void *tar_find(const void *archive, size_t archive_size,
                     const char *path, size_t *out_size)
{
    const unsigned char *base = (const unsigned char *)archive;
    const unsigned char *end = base + archive_size;

    const unsigned char *p = base;
    char long_name[TAR_LONGNAME_MAX];
    long_name[0] = '\0';

    while (p + TAR_BLOCK_SIZE <= end) {
        const tar_header_t *hdr = (const tar_header_t *)p;
        if (header_is_zero(hdr))
            break;

        unsigned long fsize = tar_parse_octal(hdr->size, sizeof(hdr->size));

        if (hdr->typeflag == 'L') {
            /* long filename entry: read name into long_name and advance */
            size_t copy_len = (fsize < (TAR_LONGNAME_MAX - 1)) ? (size_t)fsize : (TAR_LONGNAME_MAX - 1);
            const unsigned char *data = p + TAR_BLOCK_SIZE;
            if (data + copy_len > end)
                break;
            memcpy(long_name, data, copy_len);
            long_name[copy_len] = '\0';
            size_t skip = TAR_BLOCK_SIZE + round_up_512((size_t)fsize);
            if (p + skip > end)
                break;
            p += skip;
            continue;
        }

        char name[256];
        if (long_name[0]) {
            strncpy(name, long_name, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
        } else {
            build_full_name(hdr, name, sizeof(name));
        }

        if ((hdr->typeflag == '\0' || hdr->typeflag == '0') && strcmp(name, path) == 0) {
            if (out_size)
                *out_size = (size_t)fsize;
            return (const void *)(p + TAR_BLOCK_SIZE);
        }

        size_t skip = TAR_BLOCK_SIZE + round_up_512((size_t)fsize);
        if (p + skip > end)
            break;
        p += skip;
    }
    return NULL;
}
