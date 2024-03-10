/*Author: Shafin Alam*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h> /*Include for file-related system calls*/

#define MAX_LINE 80       /*80 chars per line, per command, should be enough.*/
#define BUFFER_SIZE 50
#define HISTORY 10   /*Maximum number of commands in history */

char* hist[HISTORY];  /*Array to store command history*/
int history_count = 0;


/*Function to write the history to a file using system calls*/
void writeHistory(const char* filename) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Error opening file");
        return;
    }
    int i;
    for (i = 0; i < history_count; i++) {
        size_t len = strlen(hist[i]);
        write(fd, hist[i], len);
        write(fd, "\n", 1); /*Write a newline after each command*/
    }
    close(fd);
}

/* Function to load the history from a file using system calls*/
void loadHistory(const char* filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        return; /*File does not exist yet, history will be initialized as an empty buffer*/
    }

    char line[MAX_LINE];
    int bytesRead;
    int newlinePos;
    int i = 0;

    while ((bytesRead = read(fd, line, MAX_LINE)) > 0) {
        line[bytesRead] = '\0'; /*Null-terminate the string read from the file*/

        /*Allocate new memory for the current command and copy the contents*/
        char* command = strdup(line);
        if (command == NULL) {
            perror("Error allocating memory for command");
            continue;
        }

        /*Add the command to the history*/
        if (i < HISTORY) {
            hist[i] = command;
            i++;
        } else {
            free(command); /*Free memory since the history is full*/
        }
    }

    close(fd);
    history_count = i; /*Update the history_count based on the number of commands read*/
}




/*setup() reads the user's input, separates it into tokens, and sets up the arguments.*/
void setup(char inputBuffer[], char *args[], int *background, int history_count) {
    int length,   // # of characters in the command line
        i,        // loop index for accessing inputBuffer array
        start,    // index where the beginning of next command parameter is
        ct;       // index of where to place the next parameter into args[]

    ct = 0;

    /*Read what the user enters on the command line*/
    length = read(STDIN_FILENO, inputBuffer, MAX_LINE);

    start = -1;
    if (length == 0)
        exit(0);  // ^d was entered, end of user command stream
    if (length < 0) {
        if (errno == EINTR) {
            // read() was interrupted by a signal, continue execution
            return;
        } else {
            perror("error reading the command");
            exit(-1);  // terminate with error code of -1
        }
    }

    // peep every character in the inputBuffer
    for (i = 0; i < length; i++) {
        switch (inputBuffer[i]) {
            case ' ':
            case '\t':  // Argument separators
                if (start != -1) {
                    args[ct] = &inputBuffer[start];  // Set up pointer
                    ct++;
                }
                inputBuffer[i] = '\0';  // Add a null char for string
                start = -1;
                break;

            case '\n':  // Should be the final char examined
                if (start != -1) {
                    args[ct] = &inputBuffer[start];
                    ct++;
                }
                inputBuffer[i] = '\0';
                args[ct] = NULL;  // No more arguments to this command

                // Store the command in the history
                if (history_count < HISTORY) {
                    hist[history_count] = strdup(args[0]);
                    history_count++;
                } else {
                    // Shift the history to make space for the new command
                    // free(hist[0]);
                    int j;
                    for (j = 1; j < HISTORY; j++) {
                        hist[j - 1] = hist[j];
                    }
                    hist[HISTORY - 1] = strdup(args[0]);
                }
                break;

            case '&':
                *background = 1;
                inputBuffer[i] = '\0';
                break;

            default:  // Some other character
                if (start == -1)
                    start = i;
        }
    }

    args[ct] = NULL;  // Just in case the input line was > 80
}


// handle_SIGINT() prints the history of commands and the command prompt.
void handle_SIGINT(int signum) {
    // Move the cursor to the next line
    printf("\n");

    // Print the command prompt
    printf("COMMAND->");
    fflush(stdout);

    // Print the history of commands
    printf("\nList of the most recent 10 commands:\n");
    int i = 0;
    while (hist[i] != NULL && i < HISTORY) {
        printf("%d: %s\n", i + 1, hist[i]);
        i++;
    }
    //reprompt user for additional commands
    printf("Command->");
    fflush(stdout);
    // Read the command from user input
    char input[3];
    fgets(input, sizeof(input), stdin);

    // Check if the command is "r" or "r x"
    if (input[0] == 'r' && (input[1] == '\n' && input[2] == '\n')) {
        if (history_count == 0) {
            printf("No commands in history.\n");
            return;
        }

        // Execute the most recent command
        char* command = hist[history_count - 1];
        printf("Executing command: %s\n", command);
        fflush(stdout);

        // Tokenize the command
        char* args[MAX_LINE/2+1];
        char* token = strtok(command, " \t\n");
        int ct = 0;
        while (token != NULL) {
            args[ct] = token;
            token = strtok(NULL, " \t\n");
            ct++;
        }
        args[ct] = NULL;

        // Execute the command
        execvp(args[0], args);

        // If execvp returns, an error occurred
        perror("Exec failed");
        return;
    }

}


int main(void) {
    char inputBuffer[MAX_LINE];  // Buffer to hold the command entered
    int background;              // Equals 1 if a command is followed by '&'
    char *args[MAX_LINE/2+1];    // Command line (of 80) has max of 40 arguments

    loadHistory("alam118.history");
    /* set up the signal handler */
    struct sigaction handler;
    handler.sa_handler = handle_SIGINT; 
    sigaction(SIGINT, &handler, NULL);
    while (1) {
        background = 0;
        printf("COMMAND->");
        fflush(stdout);
        setup(inputBuffer, args, &background, history_count);
        // writeHistory("alam118.history");
        // Check if the user entered "r" command
        if (history_count < HISTORY) {
            hist[history_count] = strdup(args[0]);
            history_count++;
        } else {
            // Shift the history to make space for the new command
            int i;
            for (i = 1; i < HISTORY; i++) {
                free(hist[i - 1]); // Free the memory of the previous command
                hist[i - 1] = hist[i];
            }
            hist[HISTORY - 1] = strdup(args[0]);
        }
        writeHistory("alam118.history");
        pid_t pid;
        int child_pid;
        int process;
        pid = fork();
        if (pid < 0) {
            printf("Failed to Fork :( \n");
            exit(-1);
        } else if (pid == 0) {
            execvp(args[0], args);
            printf("Exec failed :( \n");
            exit(-1);
        } else {
            if (background == 0) {
                child_pid = wait(&process);
                while (child_pid != pid) {
                    printf("Waiting...\n");
                    child_pid = wait(&process);
                }
                printf("Parent is done waiting :) \n");
            }
        }
        

    }
    // Free memory for all history commands
    int i;
    for (i = 0; i < history_count; i++) {
        free(hist[i]);
    }

    return 0;
} 