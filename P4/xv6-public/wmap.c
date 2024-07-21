#include "wmap.h"
#include "types.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "fs.h"
#include "file.h"

#define start_limit 0x60000000
#define end_limit 0x80000000

void sort(struct mapping arr[], int n) {
    int i, j, minIndex;
    struct mapping temp;
    for (i = 0; i < n - 1; i++) {
        minIndex = i;
        for (j = i + 1; j < n; j++) {
            if (arr[j].addr < arr[minIndex].addr) {
                minIndex = j;
            }
        }
        temp = arr[i];
        arr[i] = arr[minIndex];
        arr[minIndex] = temp;
    }
}

uint wmap(uint addr, int length, int flags, int fd) {
    struct proc *curproc = myproc();
    if (curproc->num_mappings >= MAX_WMMAP_INFO || length <= 0 ||
        (flags & (MAP_PRIVATE | MAP_SHARED)) == (MAP_PRIVATE | MAP_SHARED)) {
        return FAILED;
    }

    uint new_addr = start_limit;
    if (flags & MAP_FIXED) {
        if (addr < start_limit || addr >= end_limit || addr % PGSIZE != 0) {
            return FAILED;
        }
        for (int i = 0; i < curproc->num_mappings; i++) {
            uint mapping_end = curproc->mappings[i].addr + curproc->mappings[i].length;
            if ((addr >= curproc->mappings[i].addr && addr < mapping_end) ||
                (addr + length - 1 >= curproc->mappings[i].addr && addr + length - 1 < mapping_end)) {
                return FAILED;
            }
        }
        new_addr = addr;
    } else {
        for (int i = 0; i < curproc->num_mappings && new_addr + length - 1 < KERNBASE; i++) {
            uint mapping_end = curproc->mappings[i].addr + curproc->mappings[i].length;
            if (new_addr + length - 1 >= curproc->mappings[i].addr && new_addr < mapping_end) {
                new_addr = PGROUNDUP(mapping_end);
            }
        }
    }

    if (new_addr + length - 1 >= KERNBASE) {
        return FAILED;
    }

    struct mapping new_mapping = {new_addr, length, flags, fd, 0};
    curproc->mappings[curproc->num_mappings++] = new_mapping;
    sort(curproc->mappings, curproc->num_mappings);
    return new_addr;
}

int wunmap(uint addr) {
    struct proc *curproc = myproc();
    if (addr % PGSIZE != 0) {
        return FAILED;
    }
    for (int i = 0; i < curproc->num_mappings; i++) {
        if (curproc->mappings[i].addr == addr) {
            // Check for private anonymous mapping to ensure child unmappings do not affect the parent.
            if (curproc->mappings[i].flags & MAP_PRIVATE && curproc->mappings[i].flags & MAP_ANONYMOUS) {
                // Handle private anonymous unmapping logic here...
            }

            uint end = PGROUNDUP(curproc->mappings[i].addr + curproc->mappings[i].length);
            for (uint start = curproc->mappings[i].addr; start < end; start += PGSIZE) {
                pte_t *pte = walkpgdir(curproc->pgdir, (void *)start, 0);
                if (pte && (*pte & PTE_P)) {
                    if (!(curproc->mappings[i].flags & MAP_SHARED)) {
                        kfree(P2V(PTE_ADDR(*pte)));
                    }
                    *pte = 0;
                }
            }
            for (int j = i; j < curproc->num_mappings - 1; j++) {
                curproc->mappings[j] = curproc->mappings[j + 1];
            }
            curproc->num_mappings--;
            return SUCCESS;
        }
    }
    return FAILED;
}



uint wremap(uint oldaddr, int oldsize, int newsize, int flags) {
    if (oldaddr % PGSIZE != 0 || newsize <= 0) {
        return FAILED;
    }
    struct proc *curproc = myproc();
    for (int i = 0; i < curproc->num_mappings; i++) {
        if (curproc->mappings[i].addr == oldaddr) {
            int diff = newsize - oldsize;
            if (diff == 0) {
                return oldaddr;
            } else if (diff < 0) {
                curproc->mappings[i].length = newsize;
                return oldaddr;
            } else {
                uint end = oldaddr + newsize - 1;
                if (end >= KERNBASE || (i < curproc->num_mappings - 1 && end >= curproc->mappings[i + 1].addr)) {
                    return FAILED;
                }
                curproc->mappings[i].length = newsize;
                return oldaddr;
            }
        }
    }
    return FAILED;
}

int getpgdirinfo(struct pgdirinfo *pdinfo) {
    struct proc *curproc = myproc();
    if (!curproc) {
        return FAILED;
    }

    pde_t *curr_pgdir = curproc->pgdir;
    uint va = 0;
    int user_allocated_pages = 0;

    while (user_allocated_pages < MAX_UPAGE_INFO && va < KERNBASE) {
        pte_t *pte = walkpgdir(curr_pgdir, (void *)va, 0);
        if (pte && (*pte & PTE_U)) {
            pdinfo->va[user_allocated_pages] = va;
            pdinfo->pa[user_allocated_pages] = PTE_ADDR(*pte);
            pdinfo->n_upages++;
            user_allocated_pages++;
        }
        va += PGSIZE;
    }
    return SUCCESS;
}

int getwmapinfo(struct wmapinfo *wminfo) {
    struct proc *curproc = myproc();
    wminfo->total_mmaps = curproc->num_mappings;
    for (int i = 0; i < curproc->num_mappings; i++) {
        wminfo->addr[i] = curproc->mappings[i].addr;
        wminfo->length[i] = curproc->mappings[i].length;
        wminfo->n_loaded_pages[i] = curproc->mappings[i].num_pages_loaded;
    }
    return SUCCESS;
}

int handle_pagefault(uint addr) {
    struct proc *curproc = myproc();
    for (int i = 0; i < curproc->num_mappings; i++) {
        if (addr >= curproc->mappings[i].addr && addr < curproc->mappings[i].addr + curproc->mappings[i].length) {
            char *mem = kalloc();
            if (!mem) {
                return 0;
            }
            curproc->mappings[i].num_pages_loaded++;
            int success = mappages(curproc->pgdir, (void *)addr, PGSIZE, V2P(mem), PTE_U | PTE_W);
            if (success != 0) {
                kfree(mem);
                return 0;
            }
            memset(mem, 0, PGSIZE);
            return 1;
        }
    }
    return 0;
}
