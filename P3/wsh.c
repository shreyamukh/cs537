#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define DEFAULT_HISTORY_SIZE 5 // Max number of commands kept track of

// Struct to store history
struct History {
    char **commands; // Array to store history commands
    int capacity;    // Maximum number of commands that can be stored
    int start;       // Start index for the circular queue
    int end;         // End index for the circular queue
    int size;        // Current size of the history
} history;

// Struct to store local variables
struct Variable {
    char *name;
    char *value;
    struct Variable *next;
};

struct Variable *localVariables = NULL;

// Parse command and get its arguments
int parseInputCommand(char *commandArgs[], char* command) {
    char *args; // holds individual argument
    int numberOfArguments = 0;

    while ((args = strsep(&command, " ")) != NULL) {
        if (strlen(args) > 0) {
            commandArgs[numberOfArguments++] = args;
        }
    }

    commandArgs[numberOfArguments] = NULL;
    return numberOfArguments; // number of arguments found in relation to parsed command
}

// Parse command line into constituent pieces
int parseInputLine(char* commandList[], char* input) {

    char* command; // holds individual command
    int numberOfCommands = 0;

    // Find all commands in command line
    while ((command = strsep(&input, "|")) != NULL) {
        if (strlen(command) > 0) {
            commandList[numberOfCommands++] = command;
        }
    }

    commandList[numberOfCommands] = NULL;
    return numberOfCommands; // number of individual commands found in parsed input line
}

// Handles variable substitution
char* substituteVariable(char* arguments) {
    if (arguments[0] == '$') {
        char* varName = arguments + 1; // Skip '$' to get variable name
        char* value = getenv(varName); // Check for environment variable

        if (value == NULL) { // If not environment variable, check local variables
            struct Variable *current = localVariables;
            while (current != NULL) {
                if (strcmp(current->name, varName) == 0) {
                    value = current->value;
                    break;
                }
                current = current->next;
            }
        }

        if (value == NULL || value[0] == '\0') {
            return NULL; // variable not found/empty
        }
        return value; // value of found variable
    }
    return arguments; // Not a variable
}

// Execute an external command
int execExternalCommand(char *arguments[], int numArguments) {
    pid_t pid;
    int status = 0;

    // Create new array for resolved arguments
    char* resolvedArgs[numArguments + 1]; // added 1 for NULL terminator
    int resolvedArgCount = 0;

    // Resolve variables and build the new argument array without empty arguments
    for (int i = 0; i < numArguments; i++) {
        char* substitution = substituteVariable(arguments[i]);
        if (substitution && substitution[0] != '\0') { // Check for non-empty substitution
            resolvedArgs[resolvedArgCount++] = substitution;
        }
    }

    resolvedArgs[resolvedArgCount] = NULL; // NULL-terminate array

    pid = fork(); // Create a new process

    if (pid == -1) {
        printf("ERROR : fork"); // Fork failed
        exit(-1);
    }
    else if (pid == 0) {
        // Child process
        if (execvp(resolvedArgs[0], resolvedArgs) == -1) {
            printf("execvp: No such file or directory\n"); // execvp failed
            exit(-1);
        }
    }
    else {
        // Parent process
        do {
            waitpid(pid, &status, WUNTRACED); // waits for it to exit or stop
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return status; // status of executed command
}

// Execute multiple commands using PIPING
int execMultipleCommands(char *commands[], int numCommands) {
    int i, in_fd = 0;
    int fd[2];
    pid_t pid;
    int status;

    for (i = 0; i < numCommands; i++) {
        // Last command does not create new pipe
        if (i < numCommands - 1) {
            if (pipe(fd) == -1) {
                printf("ERROR : pipe");
                exit(EXIT_FAILURE);
            }
        }

        pid = fork();
        if (pid == -1) {
            printf("ERROR : fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) { // Child process
            if (in_fd != 0) {
                dup2(in_fd, 0); // in_fd - STDIN
                close(in_fd);
            }
            if (i < numCommands - 1) {
                close(fd[0]); // Close read end of the pipe in child
                dup2(fd[1], 1); // fd[1] - STDOUT
                close(fd[1]);
            }

            // Parse command to get arguments
            char *inputArguments[256];
            parseInputCommand(inputArguments, commands[i]);

            // Execute the command
            execvp(inputArguments[0], inputArguments);
            printf("ERROR : execvp"); // execvp only returns on error
            exit(EXIT_FAILURE);
        }
        else {
            // Parent process
            if (in_fd != 0) {
                close(in_fd); // Close previous input FD
            }
            in_fd = fd[0]; // Save read end of the current pipe to be input for the next command
            if (i < numCommands - 1) {
                close(fd[1]); // Close write end of the current pipe
            }
        }
    }

    // Parent waits for all child processes
    while (wait(&status) > 0);

    return 0; // success
}

// Built in command - 'exit'
void exitCommand(char *inputArguments[]) {
    if(inputArguments[1] != NULL) {
        printf("ERROR : Input arguments invalid\n");
        exit(-1);
    }
    else {
        exit(0);
    }
}

// Built in command - 'cd'
int cdCommand(char *inputArguments[]) {
    if(inputArguments[1] == NULL || inputArguments[2] != NULL) {
        printf("ERROR : Input arguments invalid\n");
        exit(-1);
    }
    else {
        if (chdir(inputArguments[1]) != 0) {
            printf("ERROR : chdir"); // chdir() system call failure
            exit(-1);
        }
        return 0;
    }
}

// Built-in command - 'export'
void exportCommand(char *inputArguments[]) {
    if (inputArguments[1] == NULL) {
        printf("ERROR: Missing argument for export.\n");
        return;
    }

    char *argCopy = strdup(inputArguments[1]); // Duplicate argument so there is no change to original
    if (!argCopy) {
        printf("ERROR: Memory allocation failed.\n");
        return;
    }

    char *rest = argCopy; // remainder of the string
    char *variable = strsep(&rest, "=");
    char *value = rest; // string after '='

    if (variable && value) {
        if (setenv(variable, value, 1) != 0) {
            printf("ERROR: Failed to set environment variable.\n");
        }
    } else {
        printf("ERROR: Invalid format for export. Expected VAR=value.\n");
    }

    free(argCopy); // Free duplicated string
}

void freeVariables() {
    struct Variable *current = localVariables;
    while (current != NULL) {
        struct Variable *next = current->next;
        free(current->name); // Free the memory allocated for the variable name
        free(current->value); // Free the memory allocated for the variable value
        free(current); // Free the memory allocated for the variable struct itself
        current = next; // Move to the next variable in the list
    }
    localVariables = NULL; // Reset the head of the list to NULL after freeing it
}

// Built-in command - 'local'
void localCommand(char *inputArguments[]) {
    char *variable = strtok(inputArguments[1], "=");
    char *value = strtok(NULL, "");

    if (value == NULL) {
        value = ""; // Handling unsetting variables
    }

    struct Variable *current = localVariables, *prev = NULL;
    while (current != NULL) {
        if (strcmp(current->name, variable) == 0) {
            free(current->value);
            current->value = strdup(value);
            return;
        }
        prev = current;
        current = current->next;
    }

    struct Variable *newVariable = malloc(sizeof(struct Variable));
    newVariable->name = strdup(variable);
    newVariable->value = strdup(value);
    newVariable->next = NULL;

    if (prev == NULL) { // first variable being added
        localVariables = newVariable;
    }
    else { // Add at the end
        prev->next = newVariable;
    }
}

// Built-in command - 'vars'
void varsCommand() {
    struct Variable *current = localVariables;
    while (current != NULL) {
        if (strlen(current->value) > 0) { // Only print if value is not empty
            printf("%s=%s\n", current->name, current->value);
        }
        current = current->next;
    }
}

// Free history buffer
void freeHistory() {
    for (int i = 0; i < history.capacity; i++) {
        if (history.commands[i] != NULL) {
            free(history.commands[i]);
        }
    }
    free(history.commands);
}

// Initializing history buffer
void initializeHistory(int capacity) {
    history.commands = malloc(sizeof(char*) * capacity);
    for(int i = 0; i < capacity; i++) {
        history.commands[i] = NULL;
    }
    history.capacity = capacity;
    history.start = 0;
    history.end = -1;
    history.size = 0;
}

// Helper function to check if the command is a built-in command
int isBuiltInCommand(const char *command) {
    // List of built-in commands to exclude from history
    const char *builtInCommands[] = {"exit", "cd", "export", "local", "vars", "history"};
    int numBuiltInCommands = sizeof(builtInCommands) / sizeof(builtInCommands[0]);

    for (int i = 0; i < numBuiltInCommands; i++) {
        if (strcmp(command, builtInCommands[i]) == 0) {
            return 1; // Command is a built-in command
        }
    }
    return 0; // Command is not a built-in command
}

// Adding/Updating commands in history
void updateHistory(const char *input) {

    // History is disabled, so no update required
    if (history.capacity == 0) {
        return;
    }

    // Not adding consecutive duplicate commands
    if (history.size > 0 && strcmp(history.commands[history.end], input) == 0) {
        return;
    }

    char *inputCopy = strdup(input); // Duplicate input for safe manipulation
    if (!inputCopy) {
        perror("ERROR : strdup failed");
        return;
    }

    // Parse first word from input to check against built-in commands
    char *firstWord = strtok(inputCopy, " ");

    // Not adding build-in command to history
    if (firstWord && isBuiltInCommand(firstWord)) {
        free(inputCopy); // Clean up duplicated input
        return;
    }

    free(inputCopy); // Clean up duplicated input

    // Free oldest command if history is full
    if (history.size == history.capacity) {
        free(history.commands[history.start]);
        history.start = (history.start + 1) % history.capacity;
        history.size--; // Decrement size as we are about to override an entry
    }

    // Move end pointer and save new command
    history.end = (history.end + 1) % history.capacity;
    history.commands[history.end] = strdup(input);
    history.size++;
}

// Executing command <n> if 'history <n>' is called
void executeHistoryCommand(int index) {
    if (index <= 0 || index > history.size) {
        return; // Invalid <n>
    }

    int cmdIndex = (history.end - index + 1 + history.capacity) % history.capacity; // Finding actual index
    char *commandToExecute = history.commands[cmdIndex]; // Command corresponding to found index

    if (commandToExecute == NULL) {
        return; // Invalid command at position <n>
    }

    // Re-parse the command to apply current variable substitutions
    char *inputArguments[256] = {NULL};
    int numArguments = parseInputCommand(inputArguments, commandToExecute);

    // Execute the command after substitution
    execExternalCommand(inputArguments, numArguments);
}

// function to print contents of history when 'history' is called
void printHistory() {
    int i, pos;
    for (i = 0; i < history.size; i++) {
        pos = (history.start + history.size - 1 - i) % history.capacity;
        printf("%d) %s\n", i + 1, history.commands[pos]);
    }
}

// function to change the size of history when 'history set <n>' is called
void setHistoryCapacity(int newCapacity) {

    if(newCapacity < 0) {
        return; // cannot set negative history
    }

    if (newCapacity == 0) {
        // Free all stored commands when resizing to 0
        for (int i = 0; i < history.size; i++) {
            free(history.commands[(history.start + i) % history.capacity]);
        }
        free(history.commands);
        history.commands = NULL;
        history.capacity = 0;
        history.start = 0;
        history.end = -1;
        history.size = 0;
    }
    else {
        char **newCommands = (char**)malloc(sizeof(char*) * newCapacity);
        if (!newCommands) {
            printf("ERROR: Memory allocation failed.\n");
            exit(EXIT_FAILURE);
        }

        int newStart = 0;
        int newEnd = -1;
        int newSize = 0;
        int currentSize = history.size;

        // Changing size if current history size is larger than new capacity
        if (history.size > newCapacity) {
            currentSize = newCapacity;
        }

        // Copy most recent commands to new history array
        for (int i = 0; i < currentSize; i++) {
            int idx = (history.start + i + history.size - currentSize) % history.capacity;
            newCommands[i] = history.commands[idx];
            newEnd++;
            newSize++;
        }

        // Free old commands array and commands that are not copied to new array
        for (int i = 0; i < history.size; i++) {
            if (i < history.size - currentSize) {
                free(history.commands[(history.start + i) % history.capacity]);
            }
        }
        free(history.commands);

        // Update history struct
        history.commands = newCommands;
        history.capacity = newCapacity;
        history.start = newStart;
        history.end = newEnd;
        history.size = newSize;
    }
}

// Built-in command - 'history'
void historyCommand(char *inputArguments[]) {
    // 'history'
    if (inputArguments[1] == NULL) {
        printHistory();
    }

    // 'history set <n>'
    else if (strcmp(inputArguments[1], "set") == 0 && inputArguments[2] != NULL) {
        int newCapacity = atoi(inputArguments[2]);
        setHistoryCapacity(newCapacity);
    }

    // 'history <n>'
    else {
        int index = atoi(inputArguments[1]);
        executeHistoryCommand(index);
    }
}

// Mode : Interactive (No file provided)
int interactiveMode() {

    // Run infinite while loop
    while(1) {

        printf("wsh> ");

        char *input = NULL;
        size_t inputLength = 0;

        // Check input for Ctrl D
        if (getline(&input, &inputLength, stdin) == -1) {
            if (feof(stdin)) {
                exit(0);
            }
            else {
                printf("ERROR : getline() did not work as expected.");
                exit(-1);
            }
        }

        // null-terminating the input
        input[strcspn(input, "\n")] = '\0';

        updateHistory(input);

        char* commandList[256] = {NULL}; // Creating list to hold commands from input line
        int numberOfCommands = parseInputLine(commandList, input); // number of commands in input line

        // Multiple commands
        if(numberOfCommands > 1) {
            execMultipleCommands(commandList, numberOfCommands);
        }

        // Individual command
        else {
            char *inputArguments[256] = {NULL}; // Contains arguments related to parsed command
            int numberOfArguments = parseInputCommand(inputArguments, input);

            // BUILT-IN COMMAND CHECK
            // Check for 'exit' command
            if(strcmp(inputArguments[0], "exit") == 0) {
                exitCommand(inputArguments);
            }

            // Check for 'cd' command
            else if(strcmp(inputArguments[0], "cd") == 0) {
                cdCommand(inputArguments);
            }

            // Check for 'export' command
            else if(strcmp(inputArguments[0], "export") == 0) {
                exportCommand(inputArguments);
            }

            // Check for 'local' command
            else if(strcmp(inputArguments[0], "local") == 0) {
                localCommand(inputArguments);
            }

            // Check for 'vars' command
            else if(strcmp(inputArguments[0], "vars") == 0) {
                varsCommand(inputArguments);
            }

            // Check for 'history' command
            else if(strcmp(inputArguments[0], "history") == 0) {
                historyCommand(inputArguments);
            }

            // Execute single external command
            else {

                execExternalCommand(inputArguments, numberOfArguments);
            }

            free(input);

        }

    }
}

int batchMode(char *batchFile) {
    char *input = NULL;
    size_t lineLength = 0;

    // Open batch file of commands in read mode
    FILE *file;
    file = fopen(batchFile, "r");

    if (file == NULL) {
        printf("ERROR : Issue in opening file");
        exit(-1);
    }

    // Iterate over file line by line
    while (getline(&input, &lineLength, file) != -1) {

        // null-terminating the input
        input[strcspn(input, "\n")] = '\0';

        updateHistory(input);

        char* commandList[256] = {NULL}; // Creating list to hold commands from input line
        int numberOfCommands = parseInputLine(commandList, input); // number of commands in input line

        // Multiple commands
        if(numberOfCommands > 1) {
            execMultipleCommands(commandList, numberOfCommands);
        }

        // Single command
        else {
            // Parse command into arguments
            char *inputArguments[256] = { NULL };
            int numArguments = parseInputCommand(inputArguments, input);

            // Check for 'exit' built-in command
            if(strcmp(inputArguments[0], "exit") == 0) {
                exitCommand(inputArguments);
            }

            // Check for 'cd' built-in command
            else if(strcmp(inputArguments[0], "cd") == 0) {
                cdCommand(inputArguments);
            }

            // Check for 'export' built-in command
            else if(strcmp(inputArguments[0], "export") == 0) {
                exportCommand(inputArguments);
            }

            // Check for 'local' built-in command
            else if(strcmp(inputArguments[0], "local") == 0) {
                localCommand(inputArguments);
            }

            // Check for 'vars' built-in command
            else if(strcmp(inputArguments[0], "vars") == 0) {
                varsCommand(inputArguments);
            }

            // Check for 'history' built-in command
            else if(strcmp(inputArguments[0], "history") == 0) {
                historyCommand(inputArguments);
            }

            // Execute external command
            else {
                execExternalCommand(inputArguments, numArguments);
            }

        }
    }

    free(input);

    // Check if Ctrl+D is encountered at end of file
    if (feof(file)) {
        fclose(file);
        exit(0);
    }

    else {
        fclose(file);
        printf("ERROR : getline");
        exit(-1);
    }
}

int main(int argc, char *argv[]) {
    initializeHistory(DEFAULT_HISTORY_SIZE); // Initializing history buffer

    // If no arguments, run in interactive mode
    if (argc == 1) {
        interactiveMode();
    }

    // If argument, run in batch mode
    else if (argc == 2) {
        batchMode(argv[1]);
    }

    else {
        printf("Invalid number of arguments\n");
        exit(-1);
    }

    freeVariables();
    freeHistory();
    return 0;
}
