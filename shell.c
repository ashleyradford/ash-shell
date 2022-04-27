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
    char **tokens; // pointer to an array of character pointers
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
        LOG("Token %zu: '%s'\n", elist_size(tokens), curr_tok);
    }

    elist_add(tokens, (char *) 0);
    LOG("Token %zu: '%s'\n", elist_size(tokens), curr_tok);
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

struct elist *setup_processes(struct elist *tokens) {
    /* Grab tokens as an array first to help brain understand */
    char **tokens_arr = (char **) elist_elements(tokens);

    struct elist *cmds = elist_create(30);
    int token_start = 0;
    LOG("elist size: %ld\n", elist_size(tokens));
    for (int i = 0; i < elist_size(tokens) - 1; i++) {

        if (strcmp(tokens_arr[i], "|") == 0) {
            tokens_arr[i] = '\0';

            /* Create command_line struct for command */
            struct command_line *cmd = malloc(sizeof(struct command_line));
            if (cmd == NULL) {
                perror("command_line malloc");
                return NULL;
            }
            
            /* Set up command_line struct */
            cmd->tokens = (tokens_arr + token_start);
            cmd->stdout_pipe = true;
            cmd->stdout_file = NULL;
            elist_add(cmds, cmd);

            LOG("cmd token: %s\n", *(cmd->tokens));
            token_start = i + 1;
        }
    }

    /* Create command_line struct for command */
    struct command_line *cmd = malloc(sizeof(struct command_line));
    if (cmd == NULL) {
        perror("command_line malloc");
        return NULL;
    }

    cmd->stdout_pipe = false;
    cmd->stdout_file = NULL; // COME BACK TO THIS
    cmd->tokens = (tokens_arr + token_start);
    elist_add(cmds, cmd);

    LOG("cmd token: %s\n", *(cmd->tokens));
    return cmds;
}

int execute_pipeline(struct elist *procs, int pos) {

    /* Sets up a pipeline piece by piece */
    struct command_line *cmd = elist_get(procs, pos);

    if (cmd->stdout_pipe) {
        int fds[2];
        pipe(fds);
        pid_t pid = fork();
        if (pid == 0) {
            /* Child - sending stdout of command to the pipe */
            LOG("child command: %s\n", *(cmd->tokens));
            close(fds[0]); // closing read
            dup2(fds[1], STDOUT_FILENO);
            close(fds[1]); // cleaning up fds
            execvp(cmd->tokens[0], cmd->tokens);
            perror("Bad command");
            exit(1);
        } else {
            /* Parent - setting up the stdin of the next process to come from the pipe */
            LOG("parent command: %s\n", *(cmd->tokens));
            close(fds[1]); // closing write
            dup2(fds[0], STDIN_FILENO);
            close(fds[0]); // cleaning up fds
            execute_pipeline(procs, pos+1);
        }
    } else {
        LOG("last command: %s\n", *(cmd->tokens));
        if (cmd->stdout_file != NULL) {
            int output = open(cmd->stdout_file, O_CREAT | O_WRONLY, 0666);
            dup2(output, STDOUT_FILENO);
        }
        execvp(cmd->tokens[0], cmd->tokens);
        close(STDIN_FILENO);    // only fails without this when single command,
                                // but a bad pipe works without this?
        perror("Bad command");
        exit(1);
    }
    return 0;
}

int main(void)
{
    signal(SIGINT, SIG_IGN);

    init_ui();

    char *command;
    while (true) {
        command = read_command();
        if (command == NULL) {
            //goto cleanup;
            free(command);
            break;
        }
        LOG("Input command: %s\n", command);
        
        /* Tokenize command */
        struct elist *tokens = tokenize(command);
        if (elist_get(tokens, 0) == NULL) {
            continue; // no command entered
        }

        /* Handle built in commands */
        int check_builtins = handle_builtins(tokens);
        if (check_builtins == -1) {
            break;
        } else if (check_builtins == 0) {
            continue;
        }

        /* Execute commands - handler process will call execute_pipeline */
        struct elist *procs = setup_processes(tokens);
        pid_t child = fork();
        if (child == 0) {
            if (execute_pipeline(procs, 0)) {
                printf("BAD");
            }
        } else {
            int status;
            wait(&status); // waiting to know that the pipeline is finished
        }

        /* We are done with command; free it */
        free(command);
    }

    //printf("Goodbye :)\n");
    return 0;
}
