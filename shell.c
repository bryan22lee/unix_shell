#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h> // For wait() and waitpid()
#include <sys/wait.h>
#include <sys/stat.h> // For open() and creat()
#include <fcntl.h>

/* Unix Shell Project
 *
 * shell.c (this file): Implement a command line interpreter, i.e. shell
 *
 * By: Chanik Bryan Lee
 */

// Struct for holding redirection args
typedef struct redir {
    char* from;
    char* to;
} redir_t;

/* Wrapper function to print string to stdout */
void myPrint(char *msg)
{
    write(STDOUT_FILENO, msg, strlen(msg));
}

/* Helper that returns if a char is whitespace or not */
int is_whitespace(char c) {
    return ((c == ' ') || (c == '\t') || (c == '\n') ||
        (c == '\v') || (c== '\f') || (c == '\r'));
}

/* Helper function for printing error message */
void printError() {
    char error_message[30] = "An error has occurred\n";
    write(STDOUT_FILENO, error_message, strlen(error_message));
}

/* Given a string, remove all whitespace, including tabs.
 * Assumes given string is intact, i.e. no whitespace between the desired
 * command string. */
char* rm_whitespace(char* str) {
    unsigned int i, count;
    count = 0;
    unsigned int length = (unsigned int) strlen(str);
    for (i=0; i<(length-1); i++) {
        if ((str[i] == ' ') || (str[i] == '\t') || (str[i] == '\n') ||
            (str[i] == '\v') || (str[i] == '\f') || (str[i] == '\r')) {
            count++; // count whitespace characters
        }
    }
    char* res = (char*) malloc((length - count) + 1);
    if (res == NULL) {
        printError();
        exit(1);
    }

    unsigned int j = 0;
    for (i=0; i<(length-1); i++) {
        if ((str[i] == ' ') || (str[i] == '\t') || (str[i] == '\n') ||
            (str[i] == '\v') || (str[i] == '\f') || (str[i] == '\r')) {
            continue;
        } else {
            res[j] = str[i];
            j++;
        }
    }
    res[length-count-1] = '\n';
    res[length-count] = '\0'; // null-terminate, last char
    return res;
}

/* Does the string have a ';' character? */
int multCommands(char* s) {
    int i, ret = 0;
    for (i=0; s[i] != '\0'; i++) {
        if (s[i] == ';') {
            ret++;
            break;
        }
    }
    return ret;
}

int numMultCommands(char* s) {
    int i, count = 0;
    for (i=0; s[i] != '\0'; i++) {
        if (s[i] == ';') count++;
    }
    return count + 1;
}

/* Is the string a real file? */
int is_file_real(char* file) {
    FILE* file_to_check = fopen(file, "r");
    if (file_to_check == NULL) return 0;
    fclose(file_to_check);
    return !0; // Convert to true
}

/* Is there a redirection character '>' in the string? */
int is_redir_in(char* str) {
    unsigned int i, strLen = strlen(str);
    int count = 0;
    for (i=0; i<strLen; i++) {
        if (str[i] == '>') return ++count;
    }
    return count;
}

/* char_in_str checks whether the given char is in the string str */
int char_in_str(char* str, char c) {
    unsigned int strLen = (unsigned int) strlen(str);
    unsigned int i;
    for (i=0; i<strLen; i++) {
        if (str[i] == 'c') return 1;
    }
    return 0;
}

// Given a string with redirection char '>', returns a struct holding its args
redir_t* redir_init(char* str) {
    char *token1, *token2, *res1, *res2;
    token1 = strtok(str, ">");
    token2 = strtok(NULL, ">");

    unsigned int strLen1, strLen2;
    strLen1 = strlen(token1);
    strLen2 = strlen(token2);

    res1 = (char*) malloc(strLen1 + 2);
    if (res1 == NULL) {
        printError();
        exit(1);
    }
    strcpy(res1, token1);
    res1[strLen1] = '\n';
    res1[strLen1 + 1] = '\0';

    // Always have newline character '\n' in there
    if (char_in_str(token2, '\n')) {
        res2 = (char*) malloc(strLen2 + 1);
        if (res2 == NULL) {
            printError();
            exit(1);
        }
        strcpy(res2, token2);
        res2[strLen2] = '\0';
    } else {
        res2 = (char*) malloc(strLen2 + 2);
        if (res2 == NULL) {
            printError();
            exit(1);
        }
        strcpy(res2, token2);
        res2[strLen2] = '\n';
        res2[strLen2 + 1] = '\0';
    }

    // Make the struct
    redir_t* redir_res = (redir_t*) malloc(sizeof(redir_t));
    if (redir_res == NULL) {
        printError();
        exit(1);
    }

    redir_res->from = res1;
    redir_res->to = res2;
    return redir_res;
}

/* Frees a redir struct from the heap */
void redir_free(redir_t *r) {
    free(r->from);
    free(r->to);
    free(r);
}

/* Does the given strin have multiple redirection characters? */
int mult_redir(char *str) {
    unsigned int i, count = 0;
    for (i=0; str[i] != '\0'; i++) {
        if (str[i] == '>') count++;
        if (count == 2) break;
    }

    return (count == 2);
}

/* Is the given command ONLY the redirection character? */
int just_redir(char *str) {
    unsigned int i = 0;
    while (is_whitespace(str[i]) && str[i] != '\n') i++;
    if (str[i] != '>') return 0;

    i++;
    if (str[i] == '\n') {
        if (str[i + 1] == '\0') return 1;
    }

    if (!is_whitespace(str[i])) {
        return 0;
    } else {
        while (is_whitespace(str[i]) && str[i] != '\n') i++;
        if (str[i] == '\n' && str[i + 1] == '\0') {
            return 1;
        }
        return 0;
    }
}

/* Given two valid string paths, append the contents of one file to another file */
int file_copy(char *filename1, char *filename2) {
    FILE *file1, *file2;
    char copyChar;

    if (!is_file_real(filename1) || !is_file_real(filename2)) {
        return 1;
    }

    file1 = fopen(filename1, "r");
    if (file1 == NULL) {
        return 1;
    }

    file2 = fopen(filename2, "a"); // "a" for append
    if (file2 == NULL) {
        return 1;
    }

    // Copy contents of file1 into file2
    for (copyChar = fgetc(file1); copyChar != EOF; copyChar = fgetc(file1)) {
        fputc(copyChar, file2);
    }

    fclose(file1);
    fclose(file2);

    return 0; /* SUCCESS */
}

/* Return wheter the given string has more arguments than 1 */
int more_args_than_one(char *str) {
    unsigned int i = 0, count = 0;
    while (str[i] != '\0' && is_whitespace(str[i])) i++;
    while (str[i] != '\0' && !is_whitespace(str[i])) i++;
    count++;
    while (str[i] != '\0' && is_whitespace(str[i])) i++;
    if (str[i] != '\0') count++;
    while (str[i] != '\0' && !is_whitespace(str[i])) i++;
    while (str[i] != '\0' && is_whitespace(str[i])) i++;
    if (str[i] != '\0') count++;
    
    return ((count - 1) > 1);
}


/* main: Runs the command line interpreter, i.e. shell */
int main(int argc, char *argv[])
{
    // Exit gracefully if 2 or more input files to the shell program
    if (argc > 2) {
        if ((is_file_real(argv[1]) && is_file_real(argv[2])) || !is_file_real(argv[1])) {
            printError();
            exit(0);
        }
    }

    char cmd_buff[514]; // Max character length
    char *pinput, *path, *tmpPath, *cd_input_path;
    unsigned int i, j, aNumber, fileCnst = 0;

    /* For file parsing purposes */
    int arg_tmp = 0;
    FILE* file;
    if (argc > 1) arg_tmp = 1;
    while (1) {
        if (argc > 1) {
            if (arg_tmp == 1) {
                file = fopen(argv[1], "r");
                /* Makes sure the input file is a valid file */
                if (file == NULL) {
                    printError();
                    exit(0);
                }
                arg_tmp = -1;
                fileCnst = 1;
            }
            // Batch mode: Get each line of the input file
            pinput = fgets(cmd_buff, 514, file);
            if (!pinput) { // NULL - end of file
                fclose(file);
                exit(0);
            }
        } else {
            char cwd[FILENAME_MAX];
            if (getcwd(cwd, sizeof(cwd)) == NULL) {
                fprintf(stderr, "Could not get current working directory\n");
                exit(1);
            }
            myPrint(strcat(cwd, "$ "));
            pinput = fgets(cmd_buff, 514, stdin);
            if (!pinput) {
                exit(0);
            }
        }

        // Command not greater than 512 characters, excluding the newline
        char fgetsVar;
        if (strlen(cmd_buff) == 513 && cmd_buff[512] != '\n') {
            if (fileCnst == 1) {
                myPrint(cmd_buff);
                while ((fgetsVar = fgetc(file)) != '\n' && (fgetsVar != EOF)) {
                    write(STDOUT_FILENO, &fgetsVar, 1);
                }
                myPrint("\n");

                // Error message
                printError();
                // Back to prompt
                continue;
            } else {
                myPrint(cmd_buff);
                while ((fgetsVar = fgetc(stdin)) != '\n' && (fgetsVar != EOF)) {
                    write(STDOUT_FILENO, &fgetsVar, 1);
                }
                myPrint("\n");

                // Error message
                printError();
                // Back to prompt
                continue;
            }
        }
        /* Initialize rm_whitespace (on the heap):
         * cmd_buff with all optional whitespace removed */
        char* no_whitespace = rm_whitespace(cmd_buff);

        // Batch mode should print the commands themselves as well, unless the line is empty
        if (arg_tmp == -1 && strcmp(no_whitespace, "\n") != 0) {
            myPrint(cmd_buff);
        }

        // Built-in command for 'exit' (in isolation / by itself)
        if ((strcmp(no_whitespace, "exit\n") == 0)) {
            free(no_whitespace);
            exit(0);
        }

        free(no_whitespace);


        // START OF COMMANDS (multiple or single)
        int cmdLen = strlen(cmd_buff);
        char* commandCopy = (char*) calloc(cmdLen + 1, 1);
        if (commandCopy == NULL) {
            // Error message
            printError();
            exit(1);
        }
        char* commandCpy_to_free = commandCopy;

        int commandCounter, commandCounterLimit;
        if (multCommands(cmd_buff)) {
            commandCounterLimit = numMultCommands(cmd_buff);
            commandCounter = 0;
        } else {
            commandCounter = 1;
            commandCounterLimit = 2;
        }

        char *word, **arr;
        unsigned int totalLenCounter = 0;

        char delim[7]; // On the stack, delim of whitespace
        delim[0] = ' ';
        delim[1] = '\t';
        delim[2] = '\n';
        delim[3] = '\v';
        delim[4] = '\f';
        delim[5] = '\r';
        delim[6] = '\0';

        redir_t *redir;
        char filename_buffer[FILENAME_MAX], *redir_ptr;
        int redirect_fd = -1, redirection_val, complex_redirection_val;

        // START of loop for commands
        while (commandCounter < commandCounterLimit) {
            redirection_val = 0;
            complex_redirection_val = 0;

            i = 0;
            if ((commandCounter + 1) < commandCounterLimit) {
                while (cmd_buff[totalLenCounter] != ';') {
                    commandCopy[i] = cmd_buff[totalLenCounter];
                    i++;
                    totalLenCounter++;
                }
                commandCopy[i] = '\n';
                commandCopy[i+1] = '\0';
                totalLenCounter++;
            } else { // Last time through case
                while (cmd_buff[totalLenCounter] != '\0') {
                    commandCopy[i] = cmd_buff[totalLenCounter];
                    i++;
                    totalLenCounter++;
                }
                commandCopy[i] = '\0';
            }

            if (just_redir(commandCopy) || mult_redir(commandCopy)) {
                printError();
                continue;
            }

            // ADJUST FOR ;;
            for (i=0; commandCopy[i] != '\0'; i++) {
                if (!is_whitespace(commandCopy[i])) break;
            }
            if (commandCopy[i] == '\0') goto fin;

            if (strstr(commandCopy, "cd") || strstr(commandCopy, "exit") || 
                strstr(commandCopy, "pwd")) {

                // START OF BUILT-IN
                no_whitespace = rm_whitespace(commandCopy);

                // Empty command line (i.e. just "\n")
                if (strcmp(no_whitespace, "\n") == 0 && arg_tmp != -1) {
                    free(no_whitespace);
                    commandCounter++;
                    continue;
                }

                // Redirection + built-in commands illegal
                if ((strstr(commandCopy, "exit") != NULL) && (strstr(commandCopy, ">") != NULL)) {
                    printError();
                    commandCounter++;
                    continue;
                } else if ((strstr(commandCopy, "cd") != NULL) && (strstr(commandCopy, ">") != NULL)) {
                    printError();
                    commandCounter++;
                    continue;
                } else if ((strstr(commandCopy, "pwd") != NULL) && (strstr(commandCopy, ">") != NULL)) {
                    printError();
                    commandCounter++;
                    continue;
                }

                // Built-in commands: pwd and exit can only be called by themselves
                if ((strstr(commandCopy, "exit") && (strcmp(no_whitespace, "exit\n") != 0) 
                    && !char_in_str(commandCopy, ';')) || (strstr(commandCopy, "pwd") && 
                    (strcmp(no_whitespace, "pwd\n") != 0) && !char_in_str(commandCopy, ';'))) {
                    free(no_whitespace);
                    printError();
                    commandCounter++;
                    continue;
                }

                // Two or more arguments to cd is an error if ';' is not used for multiple commands
                if (strstr(commandCopy, "cd")) {
                    for (i=0; commandCopy[i] != '\0'; i++) {
                        if (commandCopy[i] == ';') break;
                    }
                    if (commandCopy[i] == '\0' && more_args_than_one(commandCopy)) {
                        free(no_whitespace);
                        printError();
                        commandCounter++;
                        continue;
                    }
                }

                // Built-in commands (exit, cd, and pwd)
                if ((strcmp(no_whitespace, "exit\n") == 0)) {
                    free(no_whitespace);
                    exit(0);
                } else if ((strcmp(no_whitespace, "pwd\n") == 0)) {
                    free(no_whitespace);
                    char current_working_directory[FILENAME_MAX];
                    // On the stack
                    getcwd(current_working_directory, sizeof(current_working_directory));
                    unsigned int cwd_len = (unsigned int) strlen(current_working_directory);
                    current_working_directory[cwd_len] = '\n';
                    current_working_directory[cwd_len + 1] = '\0';
                    write(STDOUT_FILENO, current_working_directory, cwd_len + 1);
                    commandCounter++;
                    continue;
                } else if (strstr(commandCopy, "cd")) {
                    if ((strcmp(no_whitespace, "cd\n") == 0)) {
                        free(no_whitespace);
                        // Change to home directory
                        if (0 != chdir(getenv("HOME"))) {
                            printError();
                            commandCounter++;
                            continue;
                        }
                        commandCounter++;
                        continue;
                    } else if (strstr(commandCopy, "cd ")) {
                        free(no_whitespace);
                        // Parse the cd line
                        i = 0;
                        while (is_whitespace(commandCopy[i])) i++;
                        while (!is_whitespace(commandCopy[i])) i++;
                        while (is_whitespace(commandCopy[i])) i++;
                        tmpPath = &commandCopy[i];
                        aNumber = strlen(tmpPath);
                        path = (char*) calloc(aNumber + 1, 1);
                        if (path == NULL) {
                            printError();
                            exit(1);
                        }
                        strncpy(path, tmpPath, aNumber);
                        cd_input_path = rm_whitespace(path);
                        i = 0;
                        while (cd_input_path[i] != '\0') i++;
                        i--;
                        cd_input_path[i] = '\0';
                        if (0 != chdir(cd_input_path)) {
                            free(cd_input_path);
                            free(path);
                            printError();
                            commandCounter++;
                            continue;
                        } else {
                            free(cd_input_path);
                            free(path);
                            commandCounter++;
                            continue;
                        }
                    }
                }
                free(no_whitespace);
            }

            // REDIRECTION START: Make a redir_t pointer
            if (is_redir_in(commandCopy) && strstr(commandCopy, ">+") == NULL) {
                redir = redir_init(commandCopy);

                strcpy(commandCopy, redir->from);
                strcpy(filename_buffer, redir->to);
                redir_free(redir);

                // To remove all whitespace from filename_buffer string
                filename_buffer[(int) strlen(filename_buffer) - 1] = '\0';
                i = 0;
                while (is_whitespace(filename_buffer[i])) i++;
                redir_ptr = &filename_buffer[i];
                while (!is_whitespace(filename_buffer[i]) && filename_buffer[i] != '\0') i++;
                filename_buffer[i] = '\0';

                if (is_file_real(redir_ptr)) {
                    // Should not be an existing file
                    printError();
                    break;
                }
                redirection_val = 1;
            } // Advanced Redirection
            else if (strstr(commandCopy, ">+")) {
                i = 0;
                while (commandCopy[i] != '>') i++;
                commandCopy[i] = '\n';
                commandCopy[++i] = '\0';
                i++;

                j = 0;
                while (commandCopy[i] != '\0') {
                    filename_buffer[j] = commandCopy[i];
                    i++;
                    j++;
                }

                if (filename_buffer[j - 1] == '\n') {
                    filename_buffer[j] = '\0';
                } else {
                    filename_buffer[j++] = '\n';
                    filename_buffer[j] = '\0';
                }

                // To remove all whitespace from filename_buffer string
                filename_buffer[(int) strlen(filename_buffer) - 1] = '\0';
                i = 0;
                while (is_whitespace(filename_buffer[i])) i++;
                redir_ptr = &filename_buffer[i];
                while (!is_whitespace(filename_buffer[i]) && filename_buffer[i] != '\0') i++;
                filename_buffer[i] = '\0';

                redirection_val = 1;
                complex_redirection_val = 1;
            }

            // Set arrLen value
            unsigned int arrLen = i = 0;
            while (commandCopy[i] != '\0') {
                if (!is_whitespace(commandCopy[i])) {
                    arrLen++;
                    while (!is_whitespace(commandCopy[i])) {
                        i++;
                        if (commandCopy[i] == '\0') break;
                    }
                } else {
                    while (is_whitespace(commandCopy[i])) {
                        i++;
                        if (commandCopy[i] == '\0') break;
                    }
                }
            }

            /* first word */
            word = strtok(commandCopy, delim);
            arr = (char**) malloc(sizeof(char*)*(arrLen + 1));
            if (arr == NULL) {
                printError();
                exit(1);
            }
            i = 0;

            /* get the rest */
            while (word != NULL) {
                arr[i] = (char*) malloc(strlen(word)+1);
                if (arr[i] == NULL) {
                    printError();
                    exit(1);
                }
                strcpy(arr[i], word);
                i++;
                word = strtok(NULL, delim);
            }
            arr[arrLen] = NULL; // NULL terminate

            // fork() and create a new process using execvp()
            int childState;
            pid_t childName;

            if (redirection_val == 1 && complex_redirection_val == 1 && is_file_real(redir_ptr))
            {
                complex_redirection_val++;
            }

            if ((childName = fork()) == 0) { // Child process
                if (redirection_val == 1) {
                    // Just for child
                    if (complex_redirection_val == 2) complex_redirection_val--;

                    // 000666 -> All permissions - from man pages
                    if (complex_redirection_val == 0) {
                        redirect_fd = open(redir_ptr, O_CREAT | O_RDWR, 000666);
                    } else if (complex_redirection_val == 1) { // complex_redirection_val == 1

                        if (is_file_real(redir_ptr)) {
                            if (redirect_fd > 0) close(redirect_fd);

                            redirect_fd = open("bryans_special_filename.txt", O_CREAT | O_RDWR, 000666);
                        }

                        else if (!is_file_real(redir_ptr)) {
                            redirect_fd = open(redir_ptr, O_CREAT | O_RDWR, 000666);
                        }
                    }

                    if (redirect_fd < 0) {
                        for (i=0; i<arrLen; i++) {
                            free(arr[i]);
                        }
                        free(arr);
                        printError();
                        exit(0);
                    }

                    if (dup2(redirect_fd, STDOUT_FILENO) < 0) {
                        printError();
                        exit(0);
                    }
                }

                // Execute the command
                if (execvp(arr[0], arr) < 0) {
                    if (redirect_fd > 0) {
                        close(redirect_fd);
                    }
                }
                // REDIRECTION END

                if (redirect_fd >= 0) {
                    for (i=0; i<arrLen; i++) {
                        free(arr[i]);
                    }
                    free(arr);
                }

                // If execvp() is success, should not return
                printError();
                exit(1);
            } else { // Parent process
                // Wait for child process to finish
                waitpid(childName, &childState, 0);
                if (!WIFEXITED(childState)) {
                    // Error
                    printError();
                    continue;
                }
            }

            // For Advanced Redirection prepending, if the given file already exists
            if (redirect_fd > 0) close(redirect_fd);
            if (complex_redirection_val == 2) {
                // Copy contents
                if (0 != file_copy(redir_ptr, "bryans_special_filename.txt")) {
                    printError();
                    exit(0);
                }
                // Delete old
                if (0 != remove(redir_ptr)) {
                    printError();
                    exit(0);
                }
                // Rename new
                if (0 != rename("bryans_special_filename.txt", redir_ptr)) {
                    printError();
                    exit(0);
                }
            }

            for (i=0; i<arrLen; i++) {
                free(arr[i]);
            }
            free(arr);

            fin:
                commandCounter++;
        }
        // END of loop for commands

        free(commandCpy_to_free);
        // END OF COMMANDS (multiple or single)





    }
    return 0;
}
