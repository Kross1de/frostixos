#include <drivers/ps2.h>
#include <drivers/vbe.h>
#include <lib/terminal.h>
#include <mm/bitmap.h>
#include <mm/heap.h>
#include <mm/vmm.h>
#include <printf.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

extern terminal_t g_terminal;

static char *readline(char *buf, size_t buf_size) {
  size_t i = 0;
  while (i < buf_size - 1) {
    char c = ps2_get_char();
    if (c == '\n' || c == '\r') {
      buf[i] = '\0';
      printf("\n");
      return buf;
    } else if (c == '\b') {
      if (i > 0) {
        i--;
        printf("\b \b");
      }
    } else if (c >= ' ' && c <= '~') {
      buf[i++] = c;
      printf("%c", c);
    }
  }
  buf[i] = '\0';
  return buf;
}

static u32 hex_to_u32(const char *str) {
  u32 val = 0;
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

void shell_start(void) {
  char input[256];
  printf("Shell started\n");
  printf("Type 'help' for commands\n");

  while (1) {
    printf("$ ");
    readline(input, sizeof(input));

    char *cmd = strtok(input, " ");
    if (cmd == NULL) {
      continue;
    }

    if (strcmp(cmd, "help") == 0) {
      printf("Available commands:\n");
      printf("  help          - Display this help message\n");
      printf("  echo <text>   - Print the provided text\n");
      printf("  clear         - Clear the screen\n");
      printf("  heap_info     - Display heap total and free size\n");
      printf(
          "  pmm_info      - Display physical memory total and free pages\n");
      printf("  alloc_page    - Allocate a physical page and print address\n");
      printf("  free_page <hex_addr> - Free a physical page at the given "
             "address\n");
      printf("  kmalloc <size> - Allocate heap memory of given size and print "
             "pointer\n");
      printf("  kfree <hex_ptr> - Free heap memory at the given pointer\n");
      printf("  vmm_map <virt_hex> <phys_hex> <flags> - Map virtual to "
             "physical address\n");
      printf("  vmm_unmap <virt_hex> - Unmap the virtual address\n");

    } else if (strcmp(cmd, "echo") == 0) {
      char *arg = strtok(NULL, "");
      if (arg) {
        printf("%s\n", arg);
      }

    } else if (strcmp(cmd, "clear") == 0) {
      vbe_clear_screen(VBE_COLOR_BLACK);
    } else if (strcmp(cmd, "heap_info") == 0) {
      size_t total = heap_get_total_size();
      size_t free = heap_get_free_size();
      printf("Heap Total: %zu bytes, Free: %zu bytes\n", total, free);
    } else if (strcmp(cmd, "pmm_info") == 0) {
      u32 total_pages = pmm_get_total_pages();
      u32 free_pages = pmm_get_free_pages();
      printf("Physical Memory Total Pages: %u, Free Pages: %u\n", total_pages,
             free_pages);
    } else if (strcmp(cmd, "alloc_page") == 0) {
      u32 addr = pmm_alloc_page();
      if (addr != 0) {
        printf("Allocated physical page at 0x%x\n", addr);
      } else {
        printf("Failed to allocate physical page\n");
      }
    } else if (strcmp(cmd, "free_page") == 0) {
      char *arg = strtok(NULL, " ");
      if (arg) {
        u32 addr = hex_to_u32(arg);
        pmm_free_page(addr);
        printf("Freed physical page at 0x%x\n", addr);
      } else {
        printf("Usage: free_page <hex_addr>\n");
      }
    } else if (strcmp(cmd, "kmalloc") == 0) {
      char *arg = strtok(NULL, " ");
      if (arg) {
        size_t size = atoi(arg);
        void *ptr = kmalloc(size);
        if (ptr) {
          printf("Allocated %zu bytes at 0x%p\n", size, ptr);
        } else {
          printf("Failed to allocate %zu bytes\n", size);
        }
      } else {
        printf("Usage: kmalloc <size>\n");
      }
    } else if (strcmp(cmd, "kfree") == 0) {
      char *arg = strtok(NULL, " ");
      if (arg) {
        void *ptr = (void *)hex_to_u32(arg);
        kfree(ptr);
        printf("Freed memory at 0x%p\n", ptr);
      } else {
        printf("Usage: kfree <hex_ptr>\n");
      }
    } else if (strcmp(cmd, "vmm_map") == 0) {
      char *virt_str = strtok(NULL, " ");
      char *phys_str = strtok(NULL, " ");
      char *flags_str = strtok(NULL, " ");
      if (virt_str && phys_str && flags_str) {
        u32 virt = hex_to_u32(virt_str);
        u32 phys = hex_to_u32(phys_str);
        u32 flags = atoi(flags_str);
        kernel_status_t status = vmm_map_page(virt, phys, flags);
        if (status == KERNEL_OK) {
          printf("Mapped virtual 0x%x to physical 0x%x with flags %u\n", virt,
                 phys, flags);
        } else {
          printf("Failed to map virtual 0x%x to physical 0x%x\n", virt, phys);
        }
      } else {
        printf("Usage: vmm_map <virt_hex> <phys_hex> <flags>\n");
      }
    } else if (strcmp(cmd, "vmm_unmap") == 0) {
      char *virt_str = strtok(NULL, " ");
      if (virt_str) {
        u32 virt = hex_to_u32(virt_str);
        kernel_status_t status = vmm_unmap_page(virt);
        if (status == KERNEL_OK) {
          printf("Unmapped virtual address 0x%x\n", virt);
        } else {
          printf("Failed to unmap virtual address 0x%x\n", virt);
        }
      } else {
        printf("Usage: vmm_unmap <virt_hex>\n");
      }
    } else {
      printf("Unknown command: %s\n", cmd);
    }
  }
}
