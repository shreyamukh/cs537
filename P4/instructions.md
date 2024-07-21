# Administrivia

- **Due date**: March 12th, 2024 at 11:59pm.

- Projects may be turned in up to 3 days late but you will receive a penalty of
10 percentage points for every day it is turned in late.
- **Slip Days**: 
  - In case you need extra time on projects, you each will have 2 slip days for individual projects and 3 slip days for group projects (5 total slip days for the semester). After the due date we will make a copy of the handin directory for on time grading. 
  - To use slip days or turn in your assignment late you will submit your files with an additional file that contains only a single digit number, which is the number of days late your assignment is (e.g 1, 2, 3). Each consecutive day we will make a copy of any directories which contain one of these slipdays.txt files. This file must be present when you submit you final submission, or we won't know to grade your code. 
  - We will track your slip days and late submissions from project to project and begin to deduct percentages after you have used up your slip days.
  - After using up your slip days you can get up to 90% if turned in 1 day late, 80% for 2 days late, and 70% for 3 days late, but for any single assignment we won't accept submissions after the third day without an exception. This means if you use both of your individual slip days on a single assignment you can only submit that assignment one additional day late for a total of 3 days late with a 10% deduction.
  - Any exception will need to be requested from the instructors.

  - Example slipdays.txt
```
1
```
- Tests will be provided in ```~cs537-1/tests/P4```. There is a ```README.md``` file in that directory which contains the instructions to run
the tests. The tests are partially complete and your project may be graded on additional tests.
- Questions: We will be using Piazza for all questions.
- Collaboration: The assignment may be done by yourself or with one partner. Copying code from anyone else is considered cheating. [Read this](http://pages.cs.wisc.edu/~remzi/Classes/537/Spring2018/dontcheat.html) for more info on what is OK and what is not.  Please help us all have a good semester by not doing this.
- This project is to be done on the [Linux lab machines
](https://csl.cs.wisc.edu/docs/csl/2012-08-16-instructional-facilities/),
so you can learn more about programming in C on a typical UNIX-based
platform (Linux).  Your solution will be tested on these machines.
- If applicable, a **document describing online resources used** called **resources.txt** should be included in your submission. You are welcome to use resources online to help you with your assignment. **We don't recommend you use Large-Language Models such as ChatGPT.** For this course in particular we have seen these tools give close, but not quite right examples or explanations, that leave students more confused if they don't already know what the right answer is. Be aware that when you seek help from the instructional staff, we will not assist with working with these LLMs and we will expect you to be able to walk the instructional staff member through your code and logic. Online resources (e.g. stack overflow) and generative tools are transforming many industries including computer science and education.  However, if you use online sources, you are required to turn in a document describing your uses of these sources. Indicate in this document what percentage of your solution was done strictly by you and what was done utilizing these tools. Be specific, indicating sources used and how you interacted with those sources. Not giving credit to outside sources is a form of plagiarism. It can be good practice to make comments of sources in your code where that source was used. You will not be penalized for using LLMs or reading posts, but you should not create posts in online forums about the projects in the course. The majority of your code should also be written from your own efforts and you should be able to explain all the code you submit.

- **Handing it in**: 
  - Copy all xv6 files (not just the ones you changed) to `~cs537-1/handin/<cslogin>/P4/`.- after running `make clean`. The directory structure should look like the following:
 ```
handin/P4/<your cs login>/
|---- README.md 
|---- resources.txt
|---- xv6-public
      | ---- all the contents of xv6 with your modifications
```
  - **Group project handin:** each group only needs to hand in once. To submit,
    one person should put the code in their handin directory (the other should
    be empty).

# xv6 Memory Mapping

In this project, you're going to implement the `wmap` system call in xv6. Memory mapping, as the name suggests, is the process of mapping memory in a process's **"virtual"** address space to some physical memory. It is supported through a pair of system calls, `wmap` and `wunmap`. First, you call `wmap` to get a pointer to a memory region (with the size you specified) that you can access and use. When you no longer need that memory, you call `wunmap` to remove that mapping from the process's address space. You can grow or shrink the mapped memory through another system call `wremap`, which you are also going to implement.
You will also implement two system calls, `getwmapinfo` and `getpgdirinfo` for debugging and testing purposes. Please note that most of the tests rely on these two system calls. So you will fail most of the tests even if you have everything working but don't implement these system calls correctly.

# Objectives

- To implement `wmap`/`wunmap`/`wremap` system calls (simplified versions of `mmap`/`munmap`/`mremap`)
- To understand the xv6 memory layout
- To understand the relation between virtual and physical addresses
- To understand the use of the page fault handler for memory management

# Project Details

## `wmap` System Call

`wmap` has two modes of operation. First is **"anonymous"** memory allocation which has aspects similar to `malloc`. The real power of `wmap` comes through the support of **"file-backed"** mapping. Wait - what does file-backed mapping mean? It means you create a memory representation of a file. Reading data from that memory region is the same as reading data from the file. What happens if we write to memory that is backed by a file? Will it be reflected in the file? Well, that depends on the flags you use for `wmap`. When the flag `MAP_SHARED` is used, you need to write the (perhaps modified) contents of the memory back to the file upon `wunmap`.

This is the `wmap` system call signature:
```c
uint wmap(uint addr, int length, int flags, int fd);
```

* `addr`: Depending on the flags (`MAP_FIXED`, more on that later), it could be a hint for what "virtual" address `wmap` should use for the mapping, or the "virtual" address that `wmap` MUST use for the mapping.

* `length`: The length of the mapping in bytes. *It must be greater than 0.*

* `fd`: If it's a file-backed mapping, this is the file descriptor for the file to be mapped. You can assume that fd belongs to a file of type `FD_INODE`. Also, you can assume that fd was opened in `O_RDRW` mode. In case of `MAP_ANONYMOUS` (see flags), you should ignore this argument.

* `flags`: The kind of memory mapping you are requesting for. Flags can be **ORed** together (e.g., `MAP_PRIVATE | MAP_ANONYMOUS`). You should define these flags as constants in the `wmap.h` header file. Use the snippet provided in the *[Hints](#hints)* section. If you look at the man page, there are many flags for various purposes. In your implementation, you only need to implement these flags:

  a) *MAP_ANONYMOUS*: It's NOT a file-backed mapping. You can ignore the last argument (fd) if this flag is provided.

  b) *MAP_SHARED*: This flag tells wmap that the mapping is shared. You might be wondering, what does *"shared"* mean here? It will probably make much more sense if you also know about the `MAP_PRIVATE` flag. Memory mappings are copied from the parent to the child across the `fork` system call. If the mapping is `MAP_SHARED`, then changes made by the child will be visible to the parent and vice versa. However, if the mapping is `MAP_PRIVATE`, each process will have its own copy of the mapping.

  c) *MAP_PRIVATE*: Mapping is not shared. You still need to copy the mappings from parent to child, but these mappings should use different "physical" pages. In other words, the same virtual addresses are mapped in child, but to a different set of physical pages. This flag will cause modifications to memory to be invisible to other processes. Moreover, if it's a file-backed mapping, modifications to memory are NOT carried through to the underlying file. See *[Best Practices](#best-practices)* for a guide on implementation. *Between the flags `MAP_SHARED` and `MAP_PRIVATE`, one of them must be specified in flags. These two flags can NOT be used together.*

  d) *MAP_FIXED*: This has to do with the first argument of the `wmap` - the `addr` argument. Without `MAP_FIXED`, this address would be interpreted as a *hint* to where the mapping should be placed. If `MAP_FIXED` is set, then the mapping MUST be placed at exactly *"addr"*. In this project, you only implement the latter. *In other words, you don't care about the `addr` argument, unless `MAP_FIXED` has been set.* Also, *a valid `addr` will be a multiple of page size and within **0x60000000** and **0x80000000*** (see *[A Note on Address Allocation](#a-note-on-address-allocation)*).

Your implementation of `wmap` should do "lazy allocation". Lazy allocation means that you don't actually allocate any physical pages when `wmap` is called. You just keep track of the allocated region using some structure. Later, when the process tries to access that memory, a *page fault* is generated which is handled by the kernel. In the kernel, you can now allocate a physical page and let the user resume execution. Lazy allocation makes large memory mappings fast.
To set up the page fault handler, you'll add something like this to the `trap` function in `trap.c`:
```c
case T_PGFLT: // T_PGFLT = 14
    if page fault addr is part of a mapping: // lazy allocation
        // handle it
    else:
        cprintf("Segmentation Fault\n");
        // kill the process
```

You can make some simplifying assumptions to implement `wmap`:
* All mapped memory is readable/writable. If you look at the man page for `mmap`, you'll see a `prot` (protection) argument. We don't have that argument and assume `prot` to always be `PROT_READ | PROT_WRITE`.
* The maximum number of memory maps is 16.
* For file-backed mapping, you can always expect the map size to be equal to the file size in our tests.

## `wunmap` System Call
The signature of the `wunmap()` system call looks like the following:


```c
int wunmap(uint addr);
```
* `addr`: The starting address of the mapping to be removed. *It must be page aligned and the start address of some existing wmap.* 
  
`wunmap` removes the mapping starting at `addr` from the process virtual address space. If it's a file-backed mapping with `MAP_SHARED`, it writes the memory data back to the file to ensure the file remains up-to-date. So, `wunmap` does not partially unmap any mmap.


## A simple example of `wmap`
Here, we walk you through a simple end-to-end example of a call to `wmap`. You may want to take a look at the helper functions referenced in this example - you'll most likely need to use most of them in your implementation.
Suppose that we make the following call to `wmap`:
```c
uint address = wmap(0x60000000, 8192, MAP_FIXED | MAP_SHARED | MAP_ANONYMOUS, -1);
```
This is the simplest combination of flags - we're requesting a shared (`MAP_SHARED`) mapping having the length of two pages and starting at virtual address *0x60000000* (`MAP_FIXED`). Let's assume no memory mappings exist at the virtual address range *0x60000000* to *0x60002000* (8192 = 0x2000). So we can use that virtual address mapping to place the new memory map. You should keep track of these mappings in your code so you know if a range in virtual address space is free.

So far we only talked about virtual address - there needs to be some corresponding physical addresses that we can *map* these virtual addresses to. Physical addresses are managed at the granularity of a page, which is 4096 bytes long on x86. At this point you need to allocate some physical addresses to be mapped. Good news! physical memory is already managed in the xv6 kernel and you'll just need to call existing functions in the kernel to get free phsyical addresses. Bad news! you can't request a large chunk of physical memory in a single call. In other words, you can't simply tell the kernel "Give me a physical region of memory that's 8192 bytes long so I can map my virtual addresses to". As it was said before, it works at the granularity of a page (4096 bytes). So if you need 8192 bytes of physical memory, you need to call the kernel helper function twice, each time it gives you the starting address of a physical memory region that is 4096 bytes long. Needless to say that the physical addresses returned by the kernel are not necessarily contiguous in the physical address space.

Now let's continue with our example. We need two physical pages because the virtual address range we're mapping is two pages long. The kernel function `kalloc()` (defined in `kalloc.c`) can be used to get a physical page. So we call 
```c 
char *mem = kalloc()
```
and `mem` is the starting physical address of a free page we can use.

Note that we haven't actually *mapped* anything yet. We have just allocated virtual and physical addresses. To do the mapping, we need to place an entry in the page table. The `mappages` function does exactly that. In this case, the call to `mappages` would be something like the following:
```c
mappages(page_directory, 0x60000000, 4096, V2P(mem), PTE_W | PTE_U);
```
The first argument is the page table of the process to place the mapping for. Each process has a page table as defined in `struct proc` in `proc.h`. The last argument is the protection bits for this page. `PTE_U` means the page is user-accessible and `PTE_W` means it's writable. There's no flag for readable, because pages are always readable at least. For this project, you can always pass `PTE_W | PTE_U` as the last argument.
You should always apply the `V2P` (converts a virtual to physical address in the kernel) macro to the address that you get from `kalloc`. This is because of the way xv6 manages physical memory. The address returned by `kalloc` is not an actual physical address, it's the virtual address that the kernel uses to access each physical page. Yes, all physical pages are also mapped in the kernel. To get more details on this, refer to the second chapter in the xv6 manual on page tables. 
So, after the call to `mappages`, did we successfully map 8192 bytes of memory at 0x60000000? No! look at the call again - it maps only a single page. If we could get a large chunk of physical memory at once, we could have just called `mappages` once with the specified length, and get done with the mapping. Since life is not perfect, we have to map one page at a time, each time requesting a new physical page from `kalloc`. So we may complete our mapping by doing a second call:
```c
mem = kalloc();
mappages(page_directory, 0x60001000, 4096, V2P(mem), PTE_W | PTE_U);
```
Notice how we advanced the virtual address by one page (0x60001000 = 0x60000000 + 0x1000). Now you can quickly build up on this example and do the mapping in a loop, getting as many pages as necessary from `kalloc`.

Now let's see how to remove a mapping. Suppose the user accesses the pages we allocated at 0x60000000 for a while, and does a call to `wunmap`:
```c
wunmap(0x60000000);
```
First we need to find any meta data that we keep in the kernel for the mmap starting at 0x60000000, and remove that from the data structure (e.g. a linked list). Next, the page table must be modified so that the user can no longer access those pages. `walkpgdir` function can be used for that purpose. A typical call to `walkpgdir` may look like this:
```c
pte_t *pte = walkpgdir(page_directory, 0x60000000, 0);
```
It returns the page table entry that maps 0x60000000. We'll eventually need to do `*pte = 0`, which will cause any future reference to that virtual address to fail. Before that, we need to free the physical page we received from `kalloc`. Each `pte` stores a set of flags (e.g. R/W) and a physical page address, called *Page Frame Number* or *PFN* for short. Using a simple mask operation, you can extract the address part of a `pte`. Look at the macros defined in `mmu.h`. The final piece of the code will look like this:
```c
physical_address = PTE_ADDR(*pte);
kfree(P2V(physical_address));
```
We need to apply `P2V` because `kfree` (and `kalloc`, as explained before) only work with kernel virtual addresses. Only one page has been freed so far. You'll need to do the exact same calls, but this time passing a virtual address of 0x60001000 to free the second page:
```c
pte_t *pte = walkpgdir(page_directory, 0x60001000, 0);
physical_address = PTE_ADDR(*pte);
kfree(P2V(physical_address));
```


## `wremap` System Call

The signature of `wremap` is as follows:
```c
uint wremap(uint oldaddr, int oldsize, int newsize, int flags)
```
* `oldaddr`: The starting address of an existing memory map. *It must be page aligned and the start address of some existing wmap.* That means if an existing wmap starts at **0x60023000**, and its length is **0x1000**, `wremap` must be called with exactly those values for `oldaddr` and `oldsize`, otherwise `wremap` will fail. So, no partial remapping of mmaps.
* `oldsize`: The size of the mapping that starts at `oldaddr`.
* `newsize`: The size of the mapping returned by `wremap`. *It must be greater than 0.*
* `flags`: Either `0` or `MREMAP_MAYMOVE`.

`wremap` is used to grow or shrink an existing mapping. The existing mapping can be modified in-place, or moved to a new address depending on the flags: If `flags` is `0`, then `wremap` tries to grow/shrink the mapping in-place, and fails if there's not enough space. If `MREMAP_MAYMOVE` flag is set, then `wremap` should also try allocating the requested `newsize` by moving the mapping. *Note that you're allowed to move the mapping only if you can't grow it in-place.*
If `wremap` fails, the existing mapping should be left intact. In other words, you should only remove the old mapping after the new one succeeds.

## `getpgdirinfo` System Call
```c
int getpgdirinfo(struct pgdirinfo *pdinfo);
```
* `pdinfo`: A pointer to `struct pgdirinfo` that will be filled by the system call.

Add a new system call `getpgdirinfo` to retrieve information about the process address space by populating `struct pgdirinfo`. You should only gather information (either for calculating `n_pages` or returning `va`/`pa` pairs) on pages with `PTE_U` set (i.e. user pages). The only way to do that is to directly consult the page table for the process.
This system call should calculate how many physical pages are currently allocated in the current process's address space and store it in `n_upages`. It should also populate `va[MAX_UPAGE_INFO]` and `pa[MAX_UPAGE_INFO]` with the first `MAX_UPAGE_INFO` (see *[Hints](#hints)*) pages' virtual address and physical address, ordered by the virtual addresses.

## `getwmapinfo` System Call

```c
int getwmapinfo(struct wmapinfo *wminfo);
```
* `wminfo`: A pointer to `struct wmapinfo` that will be filled by the system call.

Add a new system call `getwmapinfo` to retrieve information about the process address space by populating `struct wmapinfo`. 
This system call should calculate the current number of memory maps (mmaps) in the process's address space and store the result in `total_mmaps`. It should also populate `addr[MAX_WMMAP_INFO]` and `length[MAX_WMAP_INFO]` with the address and length of each wmap. You can expect that the number of mmaps in the current process will not exceed `MAX_UPAGE_INFO`. The `n_loaded_pages[MAX_WMAP_INFO]` should store how many pages have been physically allocated for each wmap (corresponding index of `addr` and `length` arrays). This field should reflect lazy allocation.

## Return Values
`SUCCESS` and `FAILED` are defined in `wmap.h` to be 0 and -1, respectively (see *[Hints](#hints)*).

`wmap`, `wremap`: return the starting virtual address of the memory on success, and `FAILED` on failure. *That virtual address must be a multiple of page size.*

`wunmap`, `getwmapinfo`, `getpgdirinfo`: return `SUCCESS` to indicate success, and `FAILED` for failure.

## Modify `fork()` System Call
Memory mappings should be inherited by the child. To do this, you'll need to modify the `fork()` system call to copy the mappings from the parent process to the child across `fork()`. <!-- Because of lazy allocation, there might be some pages that are not yet reflected in the page table of the parent when calling `fork` (i.e. those pages have not been touched yet).  *We'll make sure to touch all the allocated pages in the parent before calling `fork`, so you need not worry about this case.* -->

You can make some simplifying assumptions here:
- Because of lazy allocation, there might be some pages that are not yet reflected in the page table of the parent when calling `fork` (i.e. those pages have not been touched yet).  *We'll make sure to touch all the allocated pages in the parent before calling `fork`, so you need not worry about this case.
- Moreover, you can assume that a child process is not going to unmap a mapping created by the parent. 
- Also, the parent process will always exit after the child process. 

## Modify `exit()` System Call
You should modify the `exit()` so that it removes all the mappings from the current process address space. This is to prevent memory leaks when a user program forgets to call `wunmap` before exiting.

## A Note on Address Allocation
If the `MAP_FIXED` flag is not set, then you'll need to find an available region in the virtual address space. Note that the starting address of that region MUST be page-aligned. To do this, you should first understand the memory layout of xv6. In xv6, higher memory addresses are always used to map the kernel. Specifically, there's a constant defined as **KERNBASE** and has the value of **0x80000000**. This means that only "virtual" addresses between 0 and **KERNBASE** are used for user space processes. Lower addresses in this range are used to map text, data and stack sections. `wmap` will use higher addresses in the user space, which are inside the heap section.
For this project, you're **REQUIRED** to only use addresses between **0x60000000** and **0x80000000** to serve `wmap` requests.

### Best Practices
We'll discuss the practices you may want to follow for the efficient implementation of memory mapping. However, following these guides is optional, and you are free to implement them in any way you want. 

For `MAP_PRIVATE`, a naive implementation may just duplicate all the physical pages at the beginning. Note that we don't want writes from one process to be visible to the other one, and that's why we duplicate the physical pages. However, an idea similar to "lazy allocation" can be applied to make it more efficient. It is called "*copy-on-write*". The idea is that as long as both processes are only reading memory pages, having a single copy of the physical pages would be sufficient. When one of the processes tries a write to a virtual page, we duplicate the physical page that the virtual page maps, so that both processes now have their own physical copy.

### Hints
You need to create a file named `wmap.h` with the following contents:
```c
// Flags for wmap
#define MAP_PRIVATE 0x0001
#define MAP_SHARED 0x0002
#define MAP_ANONYMOUS 0x0004
#define MAP_FIXED 0x0008
// Flags for remap
#define MREMAP_MAYMOVE 0x1

// When any system call fails, returns -1
#define FAILED -1
#define SUCCESS 0

// for `getpgdirinfo`
#define MAX_UPAGE_INFO 32
struct pgdirinfo {
    uint n_upages;           // the number of allocated physical pages in the process's user address space
    uint va[MAX_UPAGE_INFO]; // the virtual addresses of the allocated physical pages in the process's user address space
    uint pa[MAX_UPAGE_INFO]; // the physical addresses of the allocated physical pages in the process's user address space
};

// for `getwmapinfo`
#define MAX_WMMAP_INFO 16
struct wmapinfo {
    int total_mmaps;                    // Total number of wmap regions
    int addr[MAX_WMMAP_INFO];           // Starting address of mapping
    int length[MAX_WMMAP_INFO];         // Size of mapping
    int n_loaded_pages[MAX_WMMAP_INFO]; // Number of pages physically loaded into memory
};
```

You are strongly advised to start small for this project. Make sure you provide the support for a basic allocation, then add more complex functionality. At each step, make sure you have not broken your code that was previously working -- if so, stop and take time to see how the introduced changes caused the problem. The best way to achieve this is to use a version control system like `git`. This way, you can later refer to previous versions of your code if you break something.

Following these steps should help you with your implementation:

1) First of all, make sure you understand how xv6 does memory management. Refer to the [xv6 manual](https://pages.cs.wisc.edu/~oliphant/cs537-sp24/xv6-book-rev11.pdf). The second chapter called 'page tables' gives a good insight of the memory layout in xv6. Furthermore, it references some related source files. Looking at those sources should help you understand how mapping happens. You'll need to use those routines while implementing `wmap`. You will appreciate the time you spent on this step later.
2) Try to implement a basic `wmap`. Do not worry about file-backed mapping at the moment, just try `MAP_ANONYMOUS` | `MAP_FIXED` | `MAP_SHARED`. It should just check if that particular region asked by the user is available or not. If it is, you should map the pages in that range.
3) Implement `wunmap`. For now, just remove the mappings.
4) Implement `getwmapinfo` and `getpgdirinfo`. As mentioned earlier, most of the tests depend on these two syscalls to work.
5) Modify your `wmap` such that it's able to search for an available region in the process address space. This should make your `wmap` work without MAP_FIXED.
6) Support file-backed mapping. You'll need to change the `wmap` so that it's able to use the provided fd to find the file. You'll also need to revisit `wunmap` to write the changes made to the mapped memory back to disk when you're removing the mapping. You can assume that the offset is always 0.
7) Go for `fork()` and `exit()`. Copy mappings from parent to child across the `fork()` system call. Also, make sure you remove all mappings in `exit()`.
8) Implement MAP_PRIVATE. You'll need to change 'fork()' to behave differently if the mapping is private. Also, you'll need to revisit 'wunmap' and make sure changes are NOT reflected in the underlying file with MAP_PRIVATE set.
9) Implement `wremap`.

