#include <fcntl.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "history.h"
#include "logger.h"
#include "ui.h"
#include "elist.h"

struct command_line {
    char **tokens; // pointer to an array of character pointers??
    bool stdout_pipe;
    char *stdout_file;
};

/**
 * Retrieves the next token from a string.
 *
 * Parameters:
 * - str_ptr: maintains context in the string, i.e., where the next token in the
 *   string will be. If the function returns token N, then str_ptr will be
 *   updated to point to token N+1. To initialize, declare a char * that points
 *   to the string being tokenized. The pointer will be updated after each
 *   successive call to next_token.
 *
 * - delim: the set of characters to use as delimiters
 *
 * Returns: char pointer to the next token in the string.
 */
char *next_token(char **str_ptr, const char *delim)
{
    if (*str_ptr == NULL) {
        return NULL;
    }

    size_t tok_start = strspn(*str_ptr, delim);
    size_t tok_end = strcspn(*str_ptr + tok_start, delim);

    if (tok_end  == 0) { // zero length token, we must be done
        *str_ptr = NULL;
        return NULL;
    }

    char *current_ptr = *str_ptr + tok_start; // start of the current token
    *str_ptr += tok_start + tok_end; // shift ptr forward (to the end of the current token)

    if (**str_ptr == '\0') {
        *str_ptr = NULL;    // at last token
    } else {
        **str_ptr = '\0';   // need to terminate the token string
        (*str_ptr)++;       // point at the first character of the next token
    }

    return current_ptr;
}

struct elist *tokenize(char *command) {
    struct elist *tokens = elist_create(26);
    char *next_tok = command;
    char *curr_tok;

    /* Tokenize -- note that ' \t\n\r' will all be removed */
    while ((curr_tok = next_token(&next_tok, " \t\n\r")) != NULL) {
        elist_add(tokens, curr_tok);
        printf("Token %zu: '%s'\n", elist_size(tokens), curr_tok);
    }

    elist_add(tokens, "\0");
    //elist_add(tokens, (char *) 0); // CHECK THIS
    return tokens;
}

int handle_builtins(struct elist *tokens) {
    int status = 1;
    if (strcmp(elist_get(tokens,0), "exit") == 0) {
        status = -1;
    } else if (strcmp(elist_get(tokens,0), "cd") == 0) {
        status = 0;
        char *dir = elist_get(tokens,1);
        if (dir == NULL) {
            char *home = get_home();
            chdir(home);
            free(home);
        } else {
            chdir(dir);
        }
    }
    return status;
}

struct elist *setup_commands(struct elist *tokens) {
    struct elist *cmds = elist_create(30);
    int token_start = 0;
    for (int i = 0; i < elist_size(tokens); i++) {
        if (strcmp(elist_get(tokens, i), "|") == 0 || i == elist_size(tokens) - 1) {
            
            //printf("token start: %d\n", token_start);
            elist_set(tokens, i, "\0");

            /* Create command_line struct for command */
            struct command_line *cmd = malloc(sizeof(struct command_line));
            if (cmd == NULL) {
                perror("command_line malloc");
                return NULL;
            }

            /* Set up command_line struct */
            cmd->tokens = elist_get(tokens, token_start);

            if (strcmp(elist_get(tokens, i), "|") == 0) {
                cmd->stdout_pipe = true;
                cmd->stdout_file = NULL;
            } else {
                cmd->stdout_pipe = false;
                cmd->stdout_file = NULL; // COME BACK TO THIS !!!!!!!!!!
            }
            
            printf("Command: %s\n", cmd->tokens[0]);

            elist_add(cmds, cmd);

            // struct command_line *tmp = elist_get(cmds, 0);
            // char **tmp2 = tmp->tokens;
            // printf("%s\n", elist_get(tmp2,0));

            token_start = token_start + i + 1;
        }
    }
    //struct command_line *temp = elist_get(cmds, 0);
    //printf("AAHHHH %s\n", temp->tokens);
    return cmds;
}

// void execute_pipeline(struct command_line *cmds) {

//     printf("helllo?\n");
//     //struct command_line *temp = elist_get(cmds, 0);
//     // printf("cmds->tokens[0]: %s\n", cmds->tokens[0]);
//     // printf("cmds->tokens: %s\n", cmds->tokens);
//     /* Sets up a pipeline piece by piece */
//     printf("%d\n", cmds->stdout_pipe);

//     if (cmds->stdout_pipe) {
//         printf("i am true\n");
//     //     int fds[2];
//     //     pipe(fds);
//     //     pid_t pid = fork();
//     //     if (pid == 0) {
//     //         /* Child - sending stdout of command to the pipe */
//     //         dup2(fds[1], STDOUT_FILENO);
//     //         close(fds[0]); // closing read

//     //         printf("%s\n", cmds->tokens[0]);
//     //         execvp(cmds->tokens[0], cmds->tokens); 
//     //     } else {
//     //         /* Parent - setting up the stdin of the next process to come from the pipe */
//     //         dup2(fds[0], STDIN_FILENO);
//     //         close(fds[1]); // closing write
//     //         execute_pipeline(cmds+1);
//     //     }
//     } else {
//         printf("i am false\n");
//         // if (cmds->stdout_file != NULL) {
//         //     int output = open(cmds->stdout_file, O_CREAT | O_WRONLY, 0666);
//         //     dup2(output, STDOUT_FILENO);
//         // }
//         //printf("%s\n", cmds->tokens[0]);
//         execvp(cmds->tokens[0], cmds->tokens);
//     }
//     printf("wtf???\n");
// }

int main(void)
{
    signal(SIGINT, SIG_IGN);

    init_ui();

    char *command;
    while (true) {
        command = read_command();
        if (command == NULL) {
            break;
        }

        LOG("Input command: %s\n", command);
        
        /* Tokenize command */
        struct elist *tokens = tokenize(command);
        printf("tokens capacity: %ld\n", elist_capacity(tokens));
        printf("tokens size: %ld\n", elist_size(tokens));
        printf("tokens list 0: %s\n", (char *) elist_get(tokens, 0));
        printf("tokens list 1: %s\n", (char *) elist_get(tokens, 1));
        printf("tokens list 2: %s\n", (char *) elist_get(tokens, 2));
        if (elist_get(tokens, 0) == NULL) {
            continue;
        }

        /* Handle built in commands */
        int check_builtins = handle_builtins(tokens);
        if (check_builtins == -1) {
            break;
        } else if (check_builtins == 0) {
            continue;
        }

        /* Execute commands */
        struct elist *cmds = setup_commands(tokens);
        // struct command_line *tmp = elist_get(cmds, 0);
        // printf("%s\n", *(tmp->tokens)); // could be the way i make my tokens..not an array but an elist??

        /* Handler process that will call execute_pipeline */
        pid_t child = fork();
        if (child == 0) {
            //execvp(tmp->tokens, tmp->tokens);
            //execute_pipeline(elist_get(cmds, 0));
        } else {
            int status;
            wait(&status); // waiting to know that the pipeline is finished
        }

        puts("hello?");

        /* We are done with command; free it */
        free(command);
    }

    printf("Goodbye :)\n");
    return 0;
}
