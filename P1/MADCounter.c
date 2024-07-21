#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define ASCII_SIZE 128  // For ASCII characters 0-127

typedef struct character{
    char content;
    int asciiValue;
    int frequency;
    int initialPosition;
} CHARACTER;

CHARACTER charList[ASCII_SIZE];
int totalChars = 0; // total number of characters


 typedef struct word{
    char * contents;
    int frequency;
    int initialPosition;
    int length;
} WORD;

WORD *wordList = NULL;
int wordCount = 0; // number of words in the array
int totalWords = 0; // total number of words
int wordCapacity = 10;

typedef struct line{
    char * contents;
    int frequency;
    int initialPosition;
} LINE;

LINE *lineList = NULL;
int lineCount = 0; // number of words in the array
int totalLines = 0; // total number of words

bool wordsAdded = false;
bool linesAdded = false;
bool cFlag = false;
bool wFlag = false;
bool lFlag = false;
bool llFlag = false;
bool lwFlag = false;

void analyzeCharacters(FILE *file, FILE *output) {

    // Read characters from the file and call addCharacter for each character
    char ch;
    int position = 0; // To track position of character in file

    while ((ch = fgetc(file)) != EOF) {
        // Traverse the charList to count unique characters
        int index = (int)ch;
        if(ch < ASCII_SIZE) {
            charList[index].asciiValue = index;
            charList[index].content = ch;
            if (charList[index].frequency == 0) {
                charList[index].initialPosition = position;
            }
            charList[index].frequency++;
        }
        else printf("ERROR: ASCII Character above 127 detected.\n");
        position++;
        totalChars++;
    }

    fprintf(output, "Total Number of Chars = %d\n", totalChars);

    // Initialize a variable to keep track of unique characters
    int uniqueCount = 0;

    int i = 0;
    while(i < ASCII_SIZE) {
        if (charList[i].frequency>0) {
            uniqueCount++;
        }
        i++;
    }

    fprintf(output, "Total Unique Chars = %d\n\n", uniqueCount);


    for (int i = 0; i < ASCII_SIZE; i++) {
        if(charList[i].frequency != 0) {
            fprintf(output, "Ascii Value: %d, Char: %c, Count: %d, Initial Position: %d\n",
                   i, charList[i].content, charList[i].frequency, charList[i].initialPosition);
        }
    }
}

void initWordList() {
    wordsAdded = true;
    wordList = (WORD *)malloc(wordCapacity * sizeof(WORD));
    if (wordList == NULL) {
        printf("ERROR: Failed to allocate memory for wordList.\n");
        exit(1);
    }
    wordCount = 0;
}

void expandWordList() {
    int newCapacity = wordCapacity * 2;
    WORD *newArray = (WORD *)realloc(wordList, newCapacity * sizeof(WORD));
    if (newArray == NULL) {
        printf("ERROR: Failed to expand memory for wordList.\n");
        exit(1);
    }

    wordList = newArray;
    wordCapacity = newCapacity;
}


void addWords(const char *word, int position) {
    totalWords++;
    // Check if the word already exists in the array
    for (int i = 0; i < wordCount; i++) {
        if (strcmp(wordList[i].contents, word) == 0) {
            wordList[i].frequency++;
            return;
        }
    }

    // Expand the array if it is full
    if (wordCount == wordCapacity) {
        expandWordList();
    }

    // Allocate memory for a new WORD structure
    wordList[wordCount].contents = strdup(word); // Copy the word
    if (wordList[wordCount].contents == NULL) {
        printf("ERROR: Failed to allocate memory for word contents.\n");
        exit(1);
    }
    wordList[wordCount].frequency = 1;
    wordList[wordCount].initialPosition = position;
    wordList[wordCount].length = strlen(word);
    wordCount++;
}

void updateWordFile(FILE *file) {

    char word[256]; // Assuming that words will be less than 255 characters
    int position = 0;

    while (fscanf(file, " %255s", word) == 1) {
        addWords(word, position++);
    }
}

int compareWordStats(const void *a, const void *b) {
    WORD *wordA = (WORD *)a;
    WORD *wordB = (WORD *)b;
    return strcmp(wordA->contents, wordB->contents);
}

void sortWordList() {
    if (wordList != NULL && wordCount > 0) {
        qsort(wordList, wordCount, sizeof(WORD), compareWordStats);
    }
}

void analyzeWords(FILE *output) {
    fprintf(output, "Total Number of Words: %d\n", totalWords);
    fprintf(output, "Total Unique Words: %d\n\n", wordCount);

    for (int i = 0; i < wordCount; i++) {
            fprintf(output, "Word: %s, Freq: %d, Initial Position: %d\n",
                   wordList[i].contents, wordList[i].frequency, wordList[i].initialPosition);
    }

}

void initLineList() {
    linesAdded = true;
    lineCount = 0; // Initialize the lineCount to 0
    totalLines = 0; // Initialize the totalLines to 0
}

void removeTrailingNewline(char* contents) {
    if(contents == NULL) {
        return; // Handle NULL
    }

    size_t length = strlen(contents);

    // Check if the string is empty or consists of only a newline character
    if (length > 0 && contents[length -1 ]== '\n') {
        contents[length - 1] = '\0'; // Replace the newline with a null terminator
    }
}

void addLines(const char *line, int position) {
    totalLines++;

    // Check if the line already exists in the list
    for (int i = 0; i < lineCount; i++) {
        if (strcmp(lineList[i].contents, line) == 0) {
            lineList[i].frequency++;
            return;
        }
    }

    // Allocate memory for a new LINE structure
    lineList = (LINE *)realloc(lineList, (lineCount + 1) * sizeof(LINE));
    if (lineList == NULL) {
        printf("ERROR: Failed to reallocate memory for lineList.\n");
        exit(1);
    }

    lineList[lineCount].contents = strdup(line); // Copy the line
    if (lineList[lineCount].contents == NULL) {
        printf("ERROR: Failed to allocate memory for line contents.\n");
        exit(1);
    }
    lineList[lineCount].frequency = 1;
    lineList[lineCount].initialPosition = position;
    lineCount++;
}

void updateLineFile(FILE *file) {

    char *line = NULL;
    size_t len = 0;
    int position = 0;

    initLineList(); // Initialize the lineList and counters

    while (getline(&line, &len, file) != -1) {
        addLines(line, position++);
    }
    free(line);

}

int compareLineStats(const void *a, const void *b) {
    LINE *lineA = (LINE *)a;
    LINE *lineB = (LINE *)b;
    return strcmp(lineA->contents, lineB->contents);
}

void sortLineList() {
    if (lineList != NULL && lineCount > 0) {
        qsort(lineList, lineCount, sizeof(LINE), compareLineStats);
    }
}

void analyzeLines(FILE *output) {
    fprintf(output, "Total Number of Lines: %d\n", totalLines);
    fprintf(output, "Total Unique Lines: %d\n\n", lineCount);

    for (int i = 0; i < lineCount; i++) {
        removeTrailingNewline(lineList[i].contents);
        fprintf(output, "Line: %s, Freq: %d, Initial Position: %d\n",
               lineList[i].contents, lineList[i].frequency, lineList[i].initialPosition);
    }
}

void longestWord(FILE* output) {
    int longestWordLength = 0;

    // Finding length of longest word
    for (int i = 0; i < wordCount; i++) {
        int currentLength = wordList[i].length;
        if (currentLength > longestWordLength) {
            longestWordLength = currentLength;
        }
    }

    fprintf(output, "Longest Word is %d characters long:\n", longestWordLength);

    for (int i = 0; i < wordCount; i++) {
        if (wordList[i].length == longestWordLength) {
            fprintf(output, "\t%s\n", wordList[i].contents);
        }
    }
}

void longestLine(FILE* output) {
    int longestLineLength = 0;

        for (int i = 0; i < lineCount; i++) {
            removeTrailingNewline(lineList[i].contents);
        }

    // Finding length of longest line
    for (int i = 0; i < lineCount; i++) {
        int currentLength = strlen(lineList[i].contents);
        if (currentLength > longestLineLength) {
            longestLineLength = currentLength;
        }
    }

    fprintf(output, "Longest Line is %d characters long:\n", longestLineLength);

    for (int i = 0; i < lineCount; i++) {
        if (strlen(lineList[i].contents) == longestLineLength) {
            fprintf(output, "\t%s\n", lineList[i].contents);
        }
    }
}

void batchFileOption(char *line) {
    char BatchRun[512] = "./MADCounter ";
    strcat(BatchRun, line);
    system(BatchRun);
}

// Free dynamically allocated memory for wordList and lineList
void cleanup() {
    if (wordList != NULL) {
        for (int i = 0; i < wordCount; i++) {
            free(wordList[i].contents);
        }
        free(wordList);
    }

    if (lineList != NULL) {
        for (int i = 0; i < lineCount; i++) {
            free(lineList[i].contents);
        }
        free(lineList);
    }
}

int main(int argc, char *argv[]) {
    // Parse command-line arguments
    // parseArguments(argc, argv);

    // Usage error - less than 3 arguments provided
    if (argc < 3) {
        printf("USAGE:\n\t./MADCounter -f <input file> -o <output file> -c -w -l -Lw -Ll\n\t\tOR\n\t./MADCounter -B <batch file>\n");
        return 1; // 1 means error
    }

    // Invalid flag - flags should begin with a '-'
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            if (argv[i][1] != 'f' && argv[i][1] != 'B' && argv[i][1] != 'o' && argv[i][1] != 'c' && argv[i][1] != 'w' && argv[i][1] != 'l' && argv[i][1] != 'L' && argv[i][2] != 'l' && argv[i][2] != 'w') {
                printf("ERROR: Invalid Flag Types\n");
                return 1; // Return an error code
            }
        }
    }

    // Checking if one of the flags is "-f" or "-B"
    int found_f = 0; // Flag to show -f is found
    int found_B = 0; // Flag to show -B is found

    // Test 3
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0) {
            found_f = 1; // Found -f
            if (i < argc - 1 && argv[i + 1][0] == '-') {
                printf("ERROR: No Input File Provided\n");
                return 1; // Return an error code
            }

        }
        else if (strcmp(argv[i], "-B") == 0) {
            found_B = 1; // Found -B
            if (i < argc - 1 && argv[i + 1][0] == '-') {
                printf("ERROR: No Input File Provided2\n");
                return 1; // Return an error code
            }
        }
    }

    // checking that either -f or -B is used
    if (!found_f && !found_B) {
        printf("ERROR: No Input File Provided\n");
        return 1; // Return an error code
    }

    // checking that both -f and -B are not used
    if (found_f && found_B) {
        printf("ERROR: Both -f and -B cannot be specified together\n");
        return 1; // Return an error code
    }

    FILE *output = stdout;// Initialize fileName to NULL

    // Checking for Output File
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i < argc - 1 && argv[i + 1][0] == '-') {
                printf("ERROR: No Output File Provided\n");
                return 1; // Return an error code
            }
            else {
                output = fopen(argv[i+1], "w");
            }
        }
    }

    char *fileName = NULL; // Initialize fileName to NULL
    char *batchFileName = NULL; // Initialize batchFileName to NULL

    if(found_f) {
        // Checking for the filename after the -f flag
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-f") == 0) {
                if (i < argc - 1 && argv[i + 1][0] != '-') {
                    // Next argument is filename
                    fileName = argv[i + 1];
                } else {
                    printf("ERROR: Can't open input file\n");
                    return 1; // Return an error code
                }
            }
        }

        FILE *file = fopen(fileName, "r");

        // Checking if file can be opened
        if (file == NULL) {
            printf("ERROR: Can't open input file\n");
            return 1;
        }

        fseek(file, 0, SEEK_END); // Move the file pointer to the end of the file.
        long fileSize = ftell(file); // Get the current file pointer position, which is the file size.

        if (fileSize == 0) {
            printf("ERROR: Input File Empty\n");
            return 1;
        }
        // Looping through command line and checking for flags
        for (int i = 1; i < argc; i++) {
            fseek(file, 0, SEEK_SET);
            // -c flag
            //fseek(file, 0, SEEK_SET);
            if (strcmp(argv[i], "-c") == 0) {
                if (cFlag || wFlag || lFlag || lwFlag || llFlag)
                    fprintf(output, "\n");
                analyzeCharacters(file, output);
                cFlag = true;
            }

                // -w flag
            else if (strcmp(argv[i], "-w") == 0) {
                if (cFlag || wFlag || lFlag || lwFlag || llFlag)
                    fprintf(output, "\n");
                if (!wordsAdded) {
                    initWordList();
                    updateWordFile(file);
                    sortWordList();
                    wordsAdded = true;
                }
                wFlag = true;
                analyzeWords(output);
            }

                // -l flag
            else if (strcmp(argv[i], "-l") == 0) {
                if (cFlag || wFlag || lFlag || lwFlag || llFlag)
                    fprintf(output, "\n");
                if (!linesAdded) {
                    initLineList();
                    updateLineFile(file);
                    sortLineList();
                    linesAdded = true;
                }
                analyzeLines(output);
                lFlag = true;
            }


                // -Lw flag
            else if (strcmp(argv[i], "-Lw") == 0) {
                if (cFlag || wFlag || lFlag || lwFlag || llFlag)
                    fprintf(output, "\n");
                if (!wordsAdded) {
                    initWordList();
                    updateWordFile(file);
                    sortWordList();
                    wordsAdded = true;
                }
                lwFlag = true;
                longestWord(output);
            }

                // -Ll flag
            else if (strcmp(argv[i], "-Ll") == 0) {
                if (cFlag || wFlag || lFlag || lwFlag || llFlag)
                    fprintf(output, "\n");
                if (!linesAdded) {
                    initLineList();
                    updateLineFile(file);
                    sortLineList();
                    linesAdded = true;
                }
                llFlag = true;
                longestLine(output);
            }
        }
        fclose(file);
        // Close the output file if it's not stdout
        if (output != stdout) {
            fclose(output);
        }
    }

    else if(found_B) {

        // Checking for the batch file name after the -B flag
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-B") == 0) {
                if (i < argc - 1 && argv[i + 1][0] != '-') {
                    // Next argument is batch file name
                    batchFileName = argv[i + 1];
                } else {
                    printf("ERROR: Batch File Empty\n");
                    return 1; // Return an error code
                }
            }
        }

        FILE *batchFile = fopen(batchFileName, "r");

        // Checking if batch file can be opened
        if (batchFile == NULL) {
            printf("ERROR: Can't open batch file\n");
            return 1;
        }

        fseek(batchFile, 0, SEEK_END); // Move the file pointer to the end of the file.
        long fileSize = ftell(batchFile); // Get the current file pointer position, which is the file size.

        if (fileSize == 0) {
            printf("ERROR: Batch File Empty\n");
            return 1;
        }

        char *line = NULL;
        size_t length = 0;
        fseek(batchFile, 0, SEEK_SET);

        while(getline(&line, &length, batchFile) != -1) {
            batchFileOption(line);
        }

        free(line);
        fclose(batchFile);
    }

    cleanup();
    return 0; // Return 0 on success
}
