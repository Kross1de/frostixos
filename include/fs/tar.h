#ifndef FS_TAR_H
#define FS_TAR_H

#include <kernel/kernel.h>
#include <stddef.h>
#include <stdbool.h>

#define TAR_BLOCK_SIZE 512

/* POSIX ustar header (padded to 512 bytes) */
typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} __attribute__((packed)) tar_header_t;

/* Validate a header checksum and magic; returns true if header looks valid */
bool tar_validate_header(const tar_header_t *hdr);

/* Pretty-print list of files in a tar archive (archive: pointer to tar data,
 * archive_size: size in bytes). This function will stop if it encounters
 * a malformed or truncated archive.
 */
void tar_list(const void *archive, size_t archive_size);

/* Find a file by path inside the tar archive. Returns pointer to file data
 * (start of data following header) and sets out_size to file size. Returns
 * NULL if not found or archive is truncated.
 */
const void *tar_find(const void *archive, size_t archive_size, const char *path, size_t *out_size);

#endif