Name : Shreya Mukherjee
CS login : mukherjee
Wisc ID : smukherjee33
Email : smukherjee33@wisc.edu

Changed made to xv6 OS:

- added a system call number (22) for 'getfilename' in syscall.h
- modified the syscalls array in syscall.c and added a getfilename call
- also added a system call handler function to syscall.c
- Declared 'getfilename' system call 
- Implemented sys_getfilename function in sysfile.c
- Created user-level program getfilename.c
- Added getfilename (user program) to UPROGS in Makefile
- Added user program name to usys.S 
- Tested code with tests provided and passed
