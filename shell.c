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
    bool append;
    char *stdin_file;
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
        LOG("Token %zu: '%s'\n", elist_size(tokens) - 1, curr_tok);
    }

    elist_add(tokens, (char *) 0);
    LOG("Token %zu: '%s'\n", elist_size(tokens) - 1, curr_tok);
    return tokens;
}

int handle_builtins(struct elist *tokens) {
    if (strcmp(elist_get(tokens,0), "exit") == 0) {
        return -1;  // exit shell
    } else if (strncmp(elist_get(tokens,0), "#", 1) == 0) {
        return 0;   // ignore entire comment lines
    } else if (strcmp(elist_get(tokens,0), "cd") == 0) {
        char *dir = elist_get(tokens,1);
        if (dir == NULL) {
            char *home = get_home();
            chdir(home);
            free(home);
        } else {
            chdir(dir);
        }
        return 0;
    } 
    return 1;
}

struct elist *setup_processes(struct elist *tokens) {
    /* Grab tokens as an array first to help brain understand */
    char **tokens_arr = (char **) elist_elements(tokens);

    struct elist *cmds = elist_create(30);
    int token_start = 0;
    LOG("elist size: %ld\n", elist_size(tokens));

    bool redirect_stdin = false;
    bool redirect_stdout = false;
    bool append_flag = false;
    char *stdin_file;
    char *stdout_file;

    /* Iterating up to the last token before our null pointer */
    for (int i = 0; i < elist_size(tokens) - 1; i++) {
        /* Checking for redirection */
        if (strcmp(tokens_arr[i], "<") == 0) { 
            tokens_arr[i] = '\0';
            redirect_stdin = true;
            stdin_file = tokens_arr[i+1];
            continue;
        } else if (strcmp(tokens_arr[i], ">") == 0) {
            tokens_arr[i] = '\0';
            redirect_stdout = true;
            stdout_file = tokens_arr[i+1];
            continue;
        } else if (strcmp(tokens_arr[i], ">>") == 0) {
            tokens_arr[i] = '\0';
            redirect_stdout = true;
            append_flag = true;
            stdout_file = tokens_arr[i+1];
            continue;
        } else if (strncmp(tokens_arr[i], "#", 1) == 0) {
            tokens_arr[i] = '\0';
            i = elist_size(tokens) - 2; // jump to end of array to create command struct
        }

        /* Check if we are at last token before our null pointer or a pipe */
        if ((i == elist_size(tokens) - 2) || (strcmp(tokens_arr[i], "|") == 0)) {
            /* Set up command_line struct */
            struct command_line *cmd = malloc(sizeof(struct command_line));
            if (cmd == NULL) {
                perror("command_line malloc");
                return NULL;
            }

            /* Set up command_line struct */
            cmd->stdout_pipe = false;
            cmd->append = append_flag;
            cmd->stdin_file = redirect_stdin ? stdin_file : NULL;
            cmd->stdout_file = redirect_stdout ? stdout_file : NULL;
            cmd->tokens = tokens_arr + token_start;
            
            /* If we are at pipe, set pipe boolean to true */
            if ((i != elist_size(tokens) - 2) && strcmp(tokens_arr[i], "|") == 0) {
                tokens_arr[i] = '\0';
                cmd->stdout_pipe = true;
                token_start = i + 1;
            }            
            
            elist_add(cmds, cmd);
            LOG("cmd token: %s\n", *(cmd->tokens));
        }
    }
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
            close(STDIN_FILENO); // child proc will reset fd for parent if exec fails
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
        if (cmd->stdin_file != NULL) {
            int input = open(cmd->stdin_file, O_RDONLY, 0666);
            dup2(input, STDIN_FILENO);
        }
        if (cmd->stdout_file != NULL) {
            int output;
            if (cmd->append) {
                output = open(cmd->stdout_file, O_CREAT | O_WRONLY | O_APPEND, 0666);
            } else {
                output = open(cmd->stdout_file, O_CREAT | O_WRONLY | O_TRUNC, 0666);
            }
            dup2(output, STDOUT_FILENO);
        }
        execvp(cmd->tokens[0], cmd->tokens);
        close(STDIN_FILENO); // child proc will reset fd for parent if exec fails
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
            execute_pipeline(procs, 0);
        } else {
            int status;
            wait(&status); // waiting to know that the pipeline is finished
            set_prompt_status(status);
        }

        /* We are done with command; free it */
        free(command);
    }

    //printf("Goodbye :)\n");
    return 0;
}
