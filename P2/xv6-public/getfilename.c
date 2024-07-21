#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[]) {

    // Checking for usage error
    if(argc < 2) {
        printf(1, "ERROR: Usage - getfilename <file>\n");
        exit();
    }

    char *filename = argv[1]; // expecting this to be the file name
    char buffer[256]; // Buffer to hold the filename

    int fd = open(filename, O_RDONLY); // Open the file

    // Checking if the file can be opened
    if(fd < 0) {
        printf(1, "ERROR: cannot open %s\n", filename);
        exit();
    }

    // Call getfilename and checking return value.
    int checkResult = getfilename(fd, buffer, sizeof(buffer));

    if(checkResult < 0) {
        printf(1, "XV6_TEST_OUTPUT Open filename: %d\n", checkResult); // Display error
    }
    else {
        printf(1, "XV6_TEST_OUTPUT Open filename: %s\n", buffer); // Display filename on success
    }

    close(fd);
    exit();
}
