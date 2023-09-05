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

/* Handle builtins -- exit and empty will not be in history */
int handle_builtins(char **command)
{
    if (strcmp(*command, "exit") == 0) {
        return -1; // exit shell
    } else if (strcmp(command[0], "") == 0) {
        return 0;
    } else if (strcmp(*command, "!!") == 0) {
        free(*command);
        *command = strdup(hist_search_cnum(hist_last_cnum()));
    } else if (strncmp(*command, "!", 1) == 0) {
        char *endPtr;
        int cmd_num = (int) strtol(*(command)+1, &endPtr, 10);

        /* Check for prefix -- there should be no 0 cmd_num */
        const char *tmp;
        if (cmd_num == 0) {
            tmp = hist_search_prefix(*(command)+1);
        } else {
            tmp = hist_search_cnum(cmd_num);
        }

        /* Check if no history could be found */
        if (tmp != NULL) {
            free(*command);
            *command = strdup(tmp);
        } else {
            return 0;
        }
    }

    /* Check built ins after bangs */
    if (strncmp(*command, "#", 1) == 0) {
        return 0; // ignore entire comment lines
    } else if (strcmp(*command, "history") == 0) {
        hist_add(*command);
        hist_print();
        return 0;
    } else if (strcmp(*command, "cd") == 0) {
        hist_add(*command);
        char *home = get_home();
        chdir(home);
        free(home);
        return 0;
    } else if (strncmp(*command, "cd", 2) == 0) {
        hist_add(*command);
        char *dir = (*command + 3);
        chdir(dir);
        return 0;
    }
    return 1;
}

/* Retrieves the next token from a string */
char *next_token(char **str_ptr, const char *delim)
{
    if (*str_ptr == NULL) {
        return NULL;
    }

    size_t tok_start = strspn(*str_ptr, delim);
    size_t tok_end = strcspn(*str_ptr + tok_start, delim);

    /* Zero length token, we must be done */
    if (tok_end  == 0) {
        *str_ptr = NULL;
        return NULL;
    }

    /* Start of the current token */
    char *current_ptr = *str_ptr + tok_start;
    /* Shift ptr forward (to the end of the current token) */
    *str_ptr += tok_start + tok_end;

    if (**str_ptr == '\0') {
        /* At the last token */
        *str_ptr = NULL;
    } else {
        /* Need to terminate the token string */
        **str_ptr = '\0';
        /* Point at the first character of the next token */
        (*str_ptr)++;
    }

    return current_ptr;
}

struct elist *tokenize(char *command)
{
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

struct elist *setup_commands(struct elist *tokens)
{
    /* Grab tokens as an array first to help brain understand */
    char **tokens_arr = (char **) elist_elements(tokens);

    struct elist *cmds = elist_create(30);
    int token_start = 0;
    LOG("elist size: %zu\n", elist_size(tokens));

    bool redirect_stdin = false;
    bool redirect_stdout = false;
    bool append_flag = false;
    char *stdin_file;
    char *stdout_file;

    /* Iterating up to the last token before our null pointer */
    for (int i = 0; i < elist_size(tokens) - 1; i++) {
        /* Checking for redirection */
        if (strcmp(tokens_arr[i], "<") == 0) { 
            tokens_arr[i] = (char *) 0;
            redirect_stdin = true;
            stdin_file = tokens_arr[i+1];
            continue; // can't string compare null so go next
        } else if (strcmp(tokens_arr[i], ">") == 0) {
            tokens_arr[i] = (char *) 0;
            redirect_stdout = true;
            stdout_file = tokens_arr[i+1];
            continue;
        } else if (strcmp(tokens_arr[i], ">>") == 0) {
            tokens_arr[i] = (char *) 0;
            redirect_stdout = true;
            append_flag = true;
            stdout_file = tokens_arr[i+1];
            continue;
        } else if (strncmp(tokens_arr[i], "#", 1) == 0) {
            tokens_arr[i] = (char *) 0;
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
            if (strcmp(tokens_arr[i], "|") == 0) {
                tokens_arr[i] = (char *) 0;
                cmd->stdout_pipe = true;
                token_start = i + 1;
            }            
            
            elist_add(cmds, cmd);
            LOG("cmd tokens: %s\n", *(cmd->tokens));

            /* Reset all flags */
            redirect_stdin = false;
            redirect_stdout = false;
            append_flag = false;
        }
    }
    return cmds;
}

int execute_pipeline(struct elist *cmds, int pos)
{
    /* Sets up a pipeline piece by piece */
    struct command_line *cmd = elist_get(cmds, pos);

    /* Take care of any redirection -- will cary into child as well */
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
            execute_pipeline(cmds, pos+1);
        }
    } else {
        LOG("last command: %s\n", *(cmd->tokens));
        execvp(cmd->tokens[0], cmd->tokens);
        close(STDIN_FILENO); // child proc will reset fd for parent if exec fails
        perror("Bad command");
        exit(1);
    }
    return 0; // why do we have to include if above else has exit??
}

int main(void)
{
    /* Ignore CTRL+C signal */
    signal(SIGINT, SIG_IGN);

    /* Set up ui and history struct */
    init_ui();
    hist_init(100);

    char *command;
    while (true) {
        command = read_command();
        set_prompt_status(0); // reset prompt status

        if (command == NULL) {
            free(command);
            break;
        }
        
        /* Handle built in commands */
        int check_builtins = handle_builtins(&command);
        if (check_builtins == -1) {
            free(command);
            break;
        } else if (check_builtins == 0) {
            free(command);
            continue;
        }

        hist_add(command);
        
        /* Tokenize command */
        struct elist *tokens = tokenize(command);

        /* Execute commands - handler process will call execute_pipeline */
        struct elist *cmds = setup_commands(tokens);
        pid_t child = fork();
        if (child == 0) {
            execute_pipeline(cmds, 0);
        } else {
            int status;
            wait(&status); // waiting to know that the pipeline is finished
            set_prompt_status(status);
        }

        /* Free user commad, each command and then all tokens and cmds list */
        free(command);
        for (int i = 0; i < elist_size(cmds); i++) {
            struct command_line *cmd = elist_get(cmds, i);
            free(cmd);
        }
        elist_destroy(tokens);
        elist_destroy(cmds);
    }

    hist_destroy();
    return 0;
}
