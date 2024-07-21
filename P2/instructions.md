CS 537 Project 2 -- Creating an xv6 System Call
==============================


Administrivia
-------------

-   **Due Date**: February 6th, at 11:59pm
- Projects may be turned in up to 3 days late but you will receive a penalty of
10 percentage points for every day it is turned in late.
- **Slip Days**: 
  - In case you need extra time on projects, you each will have 2 slip days for individual projects and 3 slip days for group projects (5 total slip days for the semester). After the due date we will make a copy of the handin directory for on time grading. 
  - To use a slip days or turn in your assignment late you will submit your files with an additional file that contains only a single digit number, which is the number of days late your assignment is(e.g 1, 2, 3). Each consecutive day we will make a copy of any directories which contain one of these slipdays.txt files. This file must be present when you submit you final submission, or we won't know to grade your code. 
  - We will track your slip days and late submissions from project to project and begin to deduct percentages after you have used up your slip days.
  - After using up your slip days you can get up to 90% if turned in 1 day late, 80% for 2 days late, and 70% for 3 days late, but for any single assignment we won't accept submissions after the third days without an exception. This means if you use both of your individual slip days on a single assignment you can only submit that assignment one additional day late for a total of 3 days late with a 10% deduction.
  - Any exception will need to be requested from the instructors.

  - Example slipdays.txt
```
1
```
- Some tests are provided at ```~cs537-1/tests/P2```. There is a ```README.md``` file in that directory which contains the instructions to run
the tests. The tests are partially complete and you are encouraged to create more tests. 

- Questions: We will be using Piazza for all questions.
- Collaboration: The assignment must be done by yourself. Copying code (from others) is considered cheating. [Read this](http://pages.cs.wisc.edu/~remzi/Classes/537/Spring2018/dontcheat.html) for more info on what is OK and what is not.
Please help us all have a good semester by not doing this.
- This project is to be done on the [Linux lab machines
](https://csl.cs.wisc.edu/docs/csl/2012-08-16-instructional-facilities/),
so you can learn more about programming in C on a typical UNIX-based
platform (Linux).  Your solution will be tested on these machines.
*   **Handing it in**: See detailed handin instructions below.

## Objectives

*   Understand the xv6 OS, a simple UNIX operating system.
*   Learn to build and customize the xv6 OS
*   Understand how system calls work in general
*   Understand how to implement a new system call
*   Be able to navigate a larger codebase and find the information needed to make changes.


Project Brief
-------------

### Creating a new System Call

In this project you will add a new system call to the xv6 operating system. More specifically, you will have to implement a system call named `getfilename` with the following signature:

```
int getfilename(int fd, char* buf, int n)
```

The system keeps track of a process's open files using a number known as the file descriptor. The `open()` system call returns a file descriptor to the process. System calls which manipulate the opened file, for example `read()` and `write()`, take a file descriptor as one of their arguments. Inside xv6, hidden from the application, there is an array in the process table where the file descriptors are the indexes, and the data are open file pointers. 

The new system call you are adding takes a integer file descriptor, a character pointer, and an integer `n` which specifies the length of the passed buffer as its arguments. It must then copy the name of the file associated with that file descriptor into the character buffer pointed to by the second argument. For example, suppose a program opens a file called `helloworld.txt`, and `open()` returns 3 as the file descriptor assigned to that file. Then the program calls `getfilename()` with `3` as the file descriptor (the first argument) and a character buffer named `message` (the second argument). When `getfilename()` returns, the name `helloworld.txt` should be copied into the `message` buffer. The maximum length of this buffer should be 256, as we won't be testing for filenames longer than that.

- If the file descriptor is invalid, or there is no open file associated with that file descriptor, then `getfilename()` does not need to copy anything into the character pointer, and simply return -1. You can use the function `argfd` to validate this.
- If a user program calls the system call with a null pointer for the character buffer, it should return -1 to indicate failure.
- If the system call succeeded, the return value should be 0 to indicate success.
- You can assume we will always pass a character buffer of length 256, and set the third argument as 256 when we test your system call. 


Please note that passing parameters to syscalls and returning values from system calls is a bit tricky and does not follow the normal parameters and return semantics that we expect to see in userspace. A good starting place would be to look at how the ```chdir``` system call is implemented, and how its arguments are handled. Remember that one of the goals of this assignment is to be able to navigate a large codebase and find examples of existing code that you can base your work on. As a small hint, you might want to create a buffer inside of struct file (present in file.h) which will keep track of the names of the files which were opened using the open syscall.

### Creating a new user level program

You will also create a new user level program, also called ```getfilename``` which will call your system call and print its output in the following format ```XV6_TEST_OUTPUT Open filename: syscalloutput``` where ```syscalloutput``` is a placeholder for the output returned by your system call. You should pass the name of the file to be opened by this user level program as a command line argument, so the syntax of calling this program will be 

```
prompt> getfilename <name_of_file>
```

### Notes

Please note that we will not be testing filenames or paths longer than **256 characters** so you can create the new `name` field in `struct file` and any buffers in your user level program with a size of 256. 

Another thing to note is that we will only be calling `getfilename` on files in the same directory where the code is running, so you do not have to worry about parsing a filepath. 


## Summary of what gets turned in

*   The **entire XV6 directory** has to be turned in with modifications made in appropriate places to add the new system call and a new user program. xv6 should compile successfully when you run `make qemu-nox`. **Important: Please make sure that you run ```make clean``` before submitting the XV6 directory**.
Ensuring that you get the submission path correct is absolutely critical to the scripts being able to recognize your submission and grading appropriately. Just to drive this point home, the following tree structure depicts how the path to your submission should look like:
```
handin/<your cs login>/P2/
|---- README.md 
|---- resources.txt
|---- xv6-public
      | ---- all the contents of xv6 with your modifications, and an additional user level program called getfilename.c
```

*   Your project should (hopefully) pass the tests we supply.
*   **Your code will be tested in the CSL Linux environment (Ubuntu 22.04.3 LTS). These machines already have qemu installed. Please make sure you test it in the same environment.**
*   Include a file called README.md describing the implementation in the top level directory. This file should include your name, your cs login, you wisc ID and email, and the status of your implementation. If it all works then just say that. If there are things which don't work, let us know. Please **list the names of all the files you changed in xv6**, with a brief description of what the change was. This will **not** be graded, so do not sweat it.
*   Please note that xv6 already has a file called README, do not modify or delete this, just include a separate file called README.md with the above details, it will not impact the other readme or cause any issues. If you remove the xv6 README, it will cause compilation errors.
*   If applicable, a **document describing online resources used** called **resources.txt**. You are welcome to use resources online you help you with your assignment. **We don't recommend you use Large-Language Models such as ChatGPT.** For this course in particular we have seen these tools give close, but not quite right examples or explanations, that leave students more confused if they don't already know what the right answer is. Be aware that when you seek help from the instructional staff, we will not assist with working with these LLMs and we will expect you to be able to walk the instructional staff member through your code and logic. Online resources (e.g. stack overflow) and generative tools are transforming many industries including computer science and education.  However, if you use online sources, you are required to turn in a document describing your uses of these sources. Indicate in this document what percentage of your solution was done strictly by you and what was done utilizing these tools. Be specific, indicating sources used and how you interacted with those sources. Not giving credit to outside sources is a form of plagiarism. It can be good practice to make comments of sources in your code where that source was used. You will not be penalized for using LLMs or reading posts, but you should not create posts in online forums about the projects in the course. The majority of your code should also be written from your own efforts and you should be able to explain all the code you submit.

## Miscellaneous

### Getting xv6 up and running

The xv6 operating system is present in the `~cs537-1/xv6/` directory on CSL AFS. The directory contains instructions on how to get the operating system up and running in a `README.md` file. The directory also contains the manual for this version of the OS in the file `book-rev11.pdf`.

We encourage you to go through some resources beforehand:

1.  [Discussion video](https://www.youtube.com/watch?v=vR6z2QGcoo8&ab_channel=RemziArpaci-Dusseau) - Remzi Arpaci-Dusseau. 
2. [Discussion video](https://mediaspace.wisc.edu/media/Shivaram+Venkataraman-+Psychology105+1.30.2020+5.31.23PM/0_2ddzbo6a/150745971) - Shivaram Venkataraman.
3. [Some background on xv6 syscalls](https://github.com/remzi-arpacidusseau/ostep-projects/blob/master/initial-xv6/background.md) - Remzi Arpaci-Dusseau.

### Adding a user-level program to xv6

As an example we will add a new user level program to the xv6 operating system. After untarring XV6 (`tar -xzf ~cs537-1/xv6/xv6.tar.gz -C .`) and changing into the directory, create a new file called `test.c` with the following contents

```
#include "types.h"
#include "stat.h"
#include "user.h"
int main(void) 
{
  printf(1, "The process ID is: %d\n", getpid());
  exit();
}
```
    

Now we need to edit the Makefile so our program compiles and is included in the file system when we build XV6. Add the name of your program to `UPROGS` in the Makefile, following the format of the other entries.

Run `make qemu-nox` again and then run `test` in the xv6 shell. It should print the PID of the process.

You may want to write some new user-level programs to help test your implementation of `getfilename`.
