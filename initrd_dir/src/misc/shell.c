#include <drivers/ps2.h>
#include <drivers/vbe.h>
#include <drivers/initrd.h>
#include <fs/tar.h>
#include <lib/terminal.h>
#include <misc/logger.h>
#include <mm/bitmap.h>
#include <mm/heap.h>
#include <mm/slab.h>
#include <mm/vma.h>
#include <mm/vmm.h>
#include <printf.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

extern terminal_t g_terminal;

static mm_struct *g_test_mm = NULL;

static char *readline(char *buf, size_t buf_size)
{
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

static struct kmem_cache *find_cache_by_name(const char *name)
{
	struct list_head *pos;
	list_for_each(pos, &kmem_caches)
	{
		struct kmem_cache *cache
			= list_entry(pos, struct kmem_cache, list);
		if (strcmp(cache->name, name) == 0) {
			return cache;
		}
	}
	return NULL;
}

void shell_start(void)
{
	char input[256];
	printf("Shell started\n");
	printf("Type 'help' for commands\n");

	g_test_mm = mm_create();
	if (!g_test_mm) {
		log(LOG_ERR, "Failed to initialize test VMA address space.\n");
		return;
	}

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
            printf("  pmm_info      - Display physical memory total and free pages\n");
            printf("  alloc_page    - Allocate a physical page and print address\n");
            printf("  free_page <hex_addr> - Free a physical page at the given address\n");
            printf("  kmalloc <size> - Allocate heap memory of given size and print pointer\n");
            printf("  kfree <hex_ptr> - Free heap memory at the given pointer\n");
            printf("  vmm_map <virt_hex> <phys_hex> <flags> - Map virtual to physical address\n");
            printf("  vmm_unmap <virt_hex> - Unmap the virtual address\n");
            printf("  slab_create <name> <size> <align> - Create a slab cache\n");
            printf("  slab_destroy <name> - Destroy a slab cache\n");
            printf("  slab_alloc <name> - Allocate an object from the cache\n");
            printf("  slab_free <name> <hex_ptr> - Free an object back to the cache\n");
            printf("  slab_info <name> - Display cache statistics\n");
            printf("  vma_mmap <addr_hex> <len_decimal> <flags_decimal> - Map anonymous VMA\n");
            printf("  vma_munmap <addr_hex> <len_decimal> - Unmap VMA range\n");
            printf("  vma_info      - Display current VMAs in test address space\n");
            printf("  vma_destroy   - Destroy test VMA address space\n");
            printf("  initrd_info   - Show initrd presence and size\n");
            printf("  initrd_ls     - List files in initrd tar archive\n");
            printf("  initrd_cat <path> - Print a file from initrd\n");
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
			printf("Heap Total: %zu bytes, Free: %zu bytes\n",
			       total, free);

		} else if (strcmp(cmd, "pmm_info") == 0) {
			u32 total_pages = pmm_get_total_pages();
			u32 free_pages = pmm_get_free_pages();
			printf("Physical Memory Total Pages: %u, Free Pages: "
			       "%u\n",
			       total_pages, free_pages);

		} else if (strcmp(cmd, "alloc_page") == 0) {
			u32 addr = pmm_alloc_page();
			if (addr != 0) {
				printf("Allocated physical page at 0x%x\n",
				       addr);
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
					printf("Allocated %zu bytes at 0x%p\n",
					       size, ptr);
				} else {
					printf("Failed to allocate %zu bytes\n",
					       size);
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
				kernel_status_t status
					= vmm_map_page(virt, phys, flags);
				if (status == KERNEL_OK) {
					printf("Mapped virtual 0x%x to "
					       "physical 0x%x with flags %u\n",
					       virt, phys, flags);
				} else {
					printf("Failed to map virtual 0x%x to "
					       "physical 0x%x\n",
					       virt, phys);
				}
			} else {
				printf("Usage: vmm_map <virt_hex> <phys_hex> "
				       "<flags>\n");
			}

		} else if (strcmp(cmd, "vmm_unmap") == 0) {
			char *virt_str = strtok(NULL, " ");
			if (virt_str) {
				u32 virt = hex_to_u32(virt_str);
				kernel_status_t status = vmm_unmap_page(virt);
				if (status == KERNEL_OK) {
					printf("Unmapped virtual address "
					       "0x%x\n",
					       virt);
				} else {
					printf("Failed to unmap virtual "
					       "address 0x%x\n",
					       virt);
				}
			} else {
				printf("Usage: vmm_unmap <virt_hex>\n");
			}

		} else if (strcmp(cmd, "vma_mmap") == 0) {
			char *addr_str = strtok(NULL, " ");
			char *len_str = strtok(NULL, " ");
			char *flags_str = strtok(NULL, " ");
			if (addr_str && len_str && flags_str) {
				unsigned long addr
					= strtoul(addr_str, NULL, 16);
				size_t len = atoi(len_str);
				u32 flags = atoi(flags_str);
				unsigned long out_addr;
				kernel_status_t status = mmap_anonymous(
					g_test_mm, addr, len, flags, &out_addr);
				if (status == KERNEL_OK) {
					printf("Mapped anonymous region at "
					       "0x%lx "
					       "(requested start: 0x%lx)\n",
					       out_addr, addr);
				} else {
					printf("Failed to map: error %d\n",
					       status);
				}
			} else {
				printf("Usage: vma_mmap <addr_hex> "
				       "<len_decimal> <flags_decimal>\n");
			}

		} else if (strcmp(cmd, "vma_munmap") == 0) {
			char *addr_str = strtok(NULL, " ");
			char *len_str = strtok(NULL, " ");
			if (addr_str && len_str) {
				unsigned long addr
					= strtoul(addr_str, NULL, 16);
				size_t len = atoi(len_str);
				kernel_status_t status
					= munmap_range(g_test_mm, addr, len);
				if (status == KERNEL_OK) {
					printf("Unmapped range starting at "
					       "0x%lx (length %zu)\n",
					       addr, len);
				} else {
					printf("Failed to unmap: error %d\n",
					       status);
				}
			} else {
				printf("Usage: vma_munmap <addr_hex> "
				       "<len_decimal>\n");
			}

		} else if (strcmp(cmd, "vma_info") == 0) {
			dump_mmap(g_test_mm);

		} else if (strcmp(cmd, "vma_destroy") == 0) {
			if (g_test_mm) {
				mm_destroy(g_test_mm);
				g_test_mm = NULL;
				printf("Test VMA address space destroyed.\n");
			} else {
				printf("No test VMA address space active.\n");
			}

		} else if (strcmp(cmd, "slab_create") == 0) {
			char *name = strtok(NULL, " ");
			char *size_str = strtok(NULL, " ");
			char *align_str = strtok(NULL, " ");
			if (name && size_str && align_str) {
				u32 size = atoi(size_str);
				u32 align = atoi(align_str);
				struct kmem_cache *cache = kmem_cache_create(
					name, size, align, SLAB_FLAGS_NONE,
					NULL);
				if (cache) {
					printf("Created slab cache '%s' "
					       "(size=%u, align=%u)\n",
					       name, size, align);
				} else {
					printf("Failed to create slab cache "
					       "'%s'\n",
					       name);
				}
			} else {
				printf("Usage: slab_create <name> <size> "
				       "<align>\n");
			}

		} else if (strcmp(cmd, "slab_destroy") == 0) {
			char *name = strtok(NULL, " ");
			if (name) {
				struct kmem_cache *cache
					= find_cache_by_name(name);
				if (cache) {
					kmem_cache_destroy(cache);
					printf("Destroyed slab cache '%s'\n",
					       name);
				} else {
					printf("Slab cache '%s' not found\n",
					       name);
				}
			} else {
				printf("Usage: slab_destroy <name>\n");
			}

		} else if (strcmp(cmd, "slab_alloc") == 0) {
			char *name = strtok(NULL, " ");
			if (name) {
				struct kmem_cache *cache
					= find_cache_by_name(name);
				if (cache) {
					void *obj = kmem_cache_alloc(cache);
					if (obj) {
						printf("Allocated object from "
						       "'%s' at 0x%p\n",
						       name, obj);
					} else {
						printf("Failed to allocate "
						       "from '%s'\n",
						       name);
					}
				} else {
					printf("Slab cache '%s' not found\n",
					       name);
				}
			} else {
				printf("Usage: slab_alloc <name>\n");
			}

		} else if (strcmp(cmd, "slab_free") == 0) {
			char *name = strtok(NULL, " ");
			char *ptr_str = strtok(NULL, " ");
			if (name && ptr_str) {
				struct kmem_cache *cache
					= find_cache_by_name(name);
				if (cache) {
					void *obj = (void *)hex_to_u32(ptr_str);
					kmem_cache_free(cache, obj);
					printf("Freed object in '%s' at 0x%p\n",
					       name, obj);
				} else {
					printf("Slab cache '%s' not found\n",
					       name);
				}
			} else {
				printf("Usage: slab_free <name> <hex_ptr>\n");
			}

		} else if (strcmp(cmd, "slab_info") == 0) {
			char *name = strtok(NULL, " ");
			if (name) {
				struct kmem_cache *cache
					= find_cache_by_name(name);
				if (cache) {
					u32 full_slabs = 0, partial_slabs = 0,
					    free_slabs = 0;
					struct list_head *pos;
					list_for_each(pos, &cache->slabs_full)
						full_slabs++;
					list_for_each(pos,
						      &cache->slabs_partial)
						partial_slabs++;
					list_for_each(pos, &cache->slabs_free)
						free_slabs++;

					printf("Slab cache '%s': obj_size=%u, "
					       "objs_per_slab=%u\n",
					       name, cache->object_size,
					       cache->objects_per_slab);
					printf("Slabs: full=%u, partial=%u, "
					       "free=%u\n",
					       full_slabs, partial_slabs,
					       free_slabs);
				} else {
					printf("Slab cache '%s' not found\n",
					       name);
				}
			} else {
				printf("Usage: slab_info <name>\n");
			}

		} else if (strcmp(cmd, "initrd_info") == 0) {
			void *data = initrd_get_data();
			uint32_t size = initrd_get_size();
			if (data && size) {
				printf("INITRD present: size=%u bytes at %p\n", size, data);
			} else {
				printf("No initrd present\n");
			}

		} else if (strcmp(cmd, "initrd_ls") == 0) {
			void *data = initrd_get_data();
			uint32_t size = initrd_get_size();
			if (!data || size == 0) {
				printf("No initrd present\n");
			} else {
				tar_list(data, size);
			}

		} else if (strcmp(cmd, "initrd_cat") == 0) {
            char *path = strtok(NULL, " ");
            if (!path) {
                printf("Usage: initrd_cat <path>\n");
            } else {
                void *data = initrd_get_data();
                uint32_t size = initrd_get_size();
                if (!data || size == 0) {
                    printf("No initrd present\n");
                } else {
                    size_t fsz = 0;
                    const char *ptr = (const char *)tar_find(data, size, path, &fsz);
                    if (!ptr) {
                        printf("File not found: %s\n", path);
                    } else {
                        /* Print up to 4096 bytes to avoid flooding */
                        size_t to_print = fsz < 4096 ? fsz : 4096;
                        for (size_t i = 0; i < to_print; i++)
                            printf("%c", ptr[i]);
                        if (fsz > to_print)
                            printf("\n... (truncated %u/%u bytes)\n", (unsigned)to_print, (unsigned)fsz);
                        }
                    }
                }

		} else {
			printf("Unknown command: %s\n", cmd);
		}
	}
}