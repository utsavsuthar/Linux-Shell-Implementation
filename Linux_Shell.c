#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <ncurses.h>
#include <ctype.h>
#include <pthread.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_FILENAME_LENGTH 256

typedef struct
{
    double *vector1;
    double *vector2;
    double *result;
    int length;
} VectorOperationData;

void *add_vectors(void *arg)
{
    VectorOperationData *data = (VectorOperationData *)arg;
    //data->result = 0.0;

    for (int i = 0; i < data->length; i++)
    {
        data->result[i] = data->vector1[i] + data->vector2[i];
    }

    pthread_exit(NULL);
}

void *sub_vectors(void *arg)
{
    VectorOperationData *data = (VectorOperationData *)arg;
    //data->result = 0.0;

    for (int i = 0; i < data->length; i++)
    {
        data->result[i] = data->vector1[i] - data->vector2[i];
    }

    pthread_exit(NULL);
}

void *dot_product(void *arg)
{
    VectorOperationData *data = (VectorOperationData *)arg;
    //data->result = 0.0;

    for (int i = 0; i < data->length; i++)
    {
        data->result[i] = data->vector1[i] * data->vector2[i];
    }

    pthread_exit(NULL);
}

void execute_command(char *command)
{
    // printf("-%s-", command);
    char *args[MAX_COMMAND_LENGTH];
    int argCount = 0;

    // Tokenize the input command
    char *token = strtok(command, " ");
    while (token != NULL)
    {
        args[argCount] = token;
        token = strtok(NULL, " ");
        argCount++;
    }
    args[argCount] = NULL;

    if (argCount == 0)
    {
        // Empty command, just return
        return;
    }

    // Check if the last argument is "&" to determine if we should wait for the child
    int should_wait = 1;
    if (argCount > 1 && strcmp(args[argCount - 1], "&") == 0)
    {
        should_wait = 0;
        args[argCount - 1] = NULL;
    }

    pid_t child_pid = fork();

    if (child_pid < 0)
    {
        perror("fork");
    }

    else if (child_pid == 0)
    {
        int pipe_pos = -1;
        for (int i = 0; i < argCount; i++)
        {
            if (strcmp(args[i], "|") == 0)
            {
                pipe_pos = i;
                break;
            }
        }

        if (pipe_pos != -1)
        {
            // Split the command into two parts
            args[pipe_pos] = NULL;

            int fd[2];
            if (pipe(fd) == -1)
            {
                perror("pipe");
                exit(1);
            }

            pid_t pipe_child_pid = fork();
            if (pipe_child_pid < 0)
            {
                perror("fork");
                exit(1);
            }

            if (pipe_child_pid == 0)
            {
                // Child process for the first part of the command
                close(fd[0]);
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
                execvp(args[0], args);
                perror("execvp");
                exit(1);
            }
            else
            {
                // Parent process
                close(fd[1]);
                dup2(fd[0], STDIN_FILENO);
                close(fd[0]);

                // Execute the second part of the command
                execvp(args[pipe_pos + 1], &args[pipe_pos + 1]);
                perror("execvp");
                exit(1);
            }
        }
        else
        {
            // This is the child process
            // printf("%s", args[0]);
            execvp(args[0], args);

            // If execvp fails, print an error and exit
            printf("Invalid Command!!\n");
            exit(1);
        }
    }
    else
    {
        // This is the parent process
        if (should_wait)
        {
            int status;
            waitpid(child_pid, &status, 0);
        }
    }
}
void vi_editor(char *filename)
{
    FILE *file = fopen(filename, "r+");
    if (file == NULL)
    {
        file = fopen(filename, "w+");
        if (file == NULL)
        {
            perror("fopen");
            return;
        }
    }

    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();

    int ch;
    int x = 0, y = 0;
    int num_lines = 1;
    int num_words = 1;
    int num_chars = 0;
    int word_started = 0;

    while (1)
    {
        ch = getch();

        if (ch == KEY_UP)
        {
            if (y > 0)
            {
                y--;
            }
        }
        else if (ch == '\n')
        { // Enter key
            num_lines++;
            num_words++;
            // wmove(stdscr, y + 1, 0);
            // winsertln(stdscr);
            y++;
            x = 0;
        }
        else if (ch == KEY_DOWN)
        {
            y++;
        }
        else if (ch == KEY_LEFT)
        {
            if (x > 0)
            {
                x--;
            }
        }
        else if (ch == KEY_RIGHT)
        {
            x++;
        }
        else if (ch == 27)
        { // ESC key
            break;
        }
        else if (ch == KEY_DC)
        { // DELETE key
            delch();
        }
        else if (ch == 19)
        { // Ctrl+S for save
            if (file != NULL)
            {
                rewind(file);
                for (int i = 0; i < getmaxy(stdscr); i++)
                {
                    char line[getmaxx(stdscr) + 1];
                    mvwinnstr(stdscr, i, 0, line, getmaxx(stdscr));
                    // num_chars += strlen(line);
                    fprintf(file, "%s\n", line);
                }
                wmove(stdscr, getmaxy(stdscr), 0);
                wclrtoeol(stdscr);
                mvwprintw(stdscr, getmaxy(stdscr), 0, "File saved: %s", filename);
                wrefresh(stdscr);
                usleep(200000);
            }
        }
        else if (ch == 24)
        { // Ctrl+X for exit
            break;
        }
        else if (isalnum(ch) || isspace(ch))
        { // Insert letters, digits, and spaces
            mvwinsch(stdscr, y, x, ch);
            x++;
            if (!isspace(ch))
            {
                num_chars++;
            }
            // if (x >= getmaxx(stdscr)) {
            //     word_started = 0;  // Set flag to indicate the word has ended
            // } else if (!word_started) {
            //     word_started = 1;
            //     num_words++;
            // }
            else
            {
                num_words++;
            }
        }
        move(y, x);
        refresh();
    }

    if (file != NULL)
    {
        fclose(file);
    }
    endwin();

    printf("Number of lines: %d\n", num_lines);
    printf("Number of words: %d\n", num_words);
    printf("Number of characters: %d\n", num_chars);
}
void execute_vector_operation(const char *operation, char *file1, char *file2, int num_threads)
{
    FILE *f1 = fopen(file1, "r");
    FILE *f2 = fopen(file2, "r");

    if (f1 == NULL || f2 == NULL)
    {
        perror("fopen");
        return;
    }

    char line1[MAX_COMMAND_LENGTH];
    char line2[MAX_COMMAND_LENGTH];
    char line3[MAX_COMMAND_LENGTH];
    fgets(line1, sizeof(line1), f1);
    fgets(line2, sizeof(line2), f2);
    strcpy(line3,line1);
    int length = 0;
    char *token = strtok(line3, " ");
    while (token != NULL)
    {
        length++;
        token = strtok(NULL, " ");
    }

    double vector1[length];
    double vector2[length];
    double result[length];
    token = strtok(line1, " ");
    for (int i = 0; i < length; i++)
    {
        vector1[i] = atof(token);
        token = strtok(NULL, " ");
    }

    token = strtok(line2, " ");
    for (int i = 0; i < length; i++)
    {
        vector2[i] = atof(token);
        token = strtok(NULL, " ");
    }

    pthread_t threads[num_threads];
    VectorOperationData thread_data[num_threads];

    for (int i = 0; i < num_threads; i++)
    {
        thread_data[i].vector1 = vector1;
        thread_data[i].vector2 = vector2;
        thread_data[i].result = result;
        thread_data[i].length = length;
        if (strcmp(operation, "addvec") == 0)
        {
            pthread_create(&threads[i], NULL, add_vectors, &thread_data[i]);
        }
        else if (strcmp(operation, "subvec") == 0)
        {
            pthread_create(&threads[i], NULL, sub_vectors, &thread_data[i]);
        }
        else if (strcmp(operation, "dotprod") == 0)
        {
            pthread_create(&threads[i], NULL, dot_product, &thread_data[i]);
        }
    }

    //double result = 0.0;

    for (int i = 0; i < num_threads; i++)
    {
        pthread_join(threads[i], NULL);
        result[i] = thread_data[i].result[i];
    }
    printf("Result: \n");
    if (strcmp(operation, "dotprod") == 0){
        double final=0;
        for (int i = 0; i < length; i++){
            final+=result[i];
    }
    printf("%.2f ",final);
    }
    else{
    for (int i = 0; i < length; i++)
    {
        printf("%.2f ",result[i]);
    }}
    fclose(f1);
    fclose(f2);
}

int main()
{
    char *command;
    char temp[MAX_COMMAND_LENGTH];
    char current_dir[MAX_COMMAND_LENGTH];

    while (1)
    {
        char *line;
        getcwd(current_dir, sizeof(current_dir)); // Get the current working directory
        command = readline("\nshell>");
        // fgets(command, sizeof(command), stdin);
        while (command[strlen(command) - 1] == '\\')
        {
            command[strlen(command) - 1] = ' ';
            line = readline("...");
            command = (char *)realloc(command, strlen(command) + strlen(line) + 1);
            strcat(command, line);
            free(line);
        }
        add_history(command);
        // Remove newline character from the end of the command
        if (command[strlen(command) - 1] == '\n')
        {
            command[strlen(command) - 1] = '\0';
        }
        strcpy(temp, command);

        // printf("%s",command);
        char *args[MAX_COMMAND_LENGTH];
        int i = 0;
        int count = 0;
        char *token = strtok(command, " ");
        while (token != NULL)
        {
            // printf("%s",token);
            args[i++] = token;
            token = strtok(NULL, " ");
            count++;
        }
        args[i] = NULL;
        // printf("%s",command);
        //  printf("%s",command);
        //   Handle built-in commands
        if (i == 0)
        {
            // Empty command, just return
            continue;
        }
        if (strncmp(temp, "addvec", 6) == 0 || strncmp(temp, "subvec", 6) == 0 || strncmp(temp, "dotprod", 7) == 0)
        {

            if (count >= 3)
            {
                char *operation = args[0];
                char *file1 = args[1];
                char *file2 = args[2];
                int num_threads = 3; // Default number of threads

                // Check for optional number of threads
                if (count >= 4 && args[3][0] == '-')
                {
                    num_threads = atoi(args[3] + 1); // Skip the '-' character
                }

                execute_vector_operation(operation, file1, file2, num_threads);
            }
            else
            {
                printf("Invalid usage. Correct usage: %s <file_1> <file_2> -<no_thread>\n", args[0]);
            }
        }
        else
        {
            if (strncmp(temp, "vi ", 3) == 0)
            {
                char filename[MAX_FILENAME_LENGTH];
                int ret = sscanf(temp, "vi %255s", filename);
                if (ret == 1)
                {
                    vi_editor(filename);
                }
                else
                {
                    printf("Invalid usage. Correct usage: vi <filename>\n");
                }
            }
            else
            {
                if (strcmp(command, "exit") == 0)
                {
                    break;
                }
                else if (strcmp(command, "help") == 0)
                {
                    printf("Available commands:\n");
                    printf("1. pwd\n");
                    printf("2. cd <directory_name>\n");
                    printf("3. mkdir <directory_name>\n");
                    printf("4. ls <flag>\n");
                    printf("5. exit\n");
                    printf("6. help\n");
                }
                else if (strcmp(args[0], "cd") == 0)
                {
                    if (count < 2)
                    {
                        printf("Usage: %s <directory>\n", args[0]);
                        exit(EXIT_FAILURE);
                    }

                    // Attempt to change the current working directory
                    if (chdir(args[1]) != 0)
                    {
                        // printf("%s", args[1]);
                        perror(":");
                        exit(EXIT_FAILURE);
                    }
                    else
                    {
                        printf("Directory changed to %s\n", args[1]);
                        // exit(EXIT_SUCCESS);
                    }
                }
                else if (strncmp(command, "ls", 2) == 0)
                {
                    if (args[1] != NULL)
                    {
                        execute_command(temp);
                    }
                    else
                    {
                        execute_command("ls"); // If no options provided, execute "ls" without options
                    }
                }
                else
                {
                    // Execute the user-provided command
                    execute_command(temp);
                }
            }
        }
    }
    return 0;
}
