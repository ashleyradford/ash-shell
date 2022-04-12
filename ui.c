#include <stdio.h>
#include <readline/readline.h>
#include <locale.h>
#include <stdlib.h>

#include "history.h"
#include "logger.h"
#include "ui.h"

static const char *good_str = "😌";
static const char *bad_str  = "🤯";

static int readline_init(void);

void init_ui(void)
{
    LOGP("Initializing UI...\n");

    char *locale = setlocale(LC_ALL, "en_US.UTF-8");
    LOG("Setting locale: %s\n",
            (locale != NULL) ? locale : "could not set locale!");

    rl_startup_hook = readline_init;
}

void destroy_ui(void)
{
    // TODO cleanup code, if necessary
}

char *prompt_line(void)
{
    const char *status = prompt_status() ? bad_str : good_str;

    char cmd_num[25];
    snprintf(cmd_num, 25, "%d", prompt_cmd_num());

    char *user = prompt_username();
    char *host = prompt_hostname();
    char *cwd = prompt_cwd();

    char *format_str = ">>-[%s]-[%s]-[%s@%s:%s]-> ";

    size_t prompt_sz
        = strlen(format_str)
        + strlen(status)
        + strlen(cmd_num)
        + strlen(user)
        + strlen(host)
        + strlen(cwd)
        + 1;

    char *prompt_str =  malloc(sizeof(char) * prompt_sz);

    snprintf(prompt_str, prompt_sz, format_str,
            status,
            cmd_num,
            user,
            host,
            cwd);

    return prompt_str;
}

char *prompt_username(void)
{
    return "unknown_user";
}

char *prompt_hostname(void)
{
    return "unknown_host";
}

char *prompt_cwd(void)
{
    return "/unknown/path";
}

int prompt_status(void)
{
    return -1;
}

unsigned int prompt_cmd_num(void)
{
    return 0;
}

char *read_command(void)
{
    char *prompt = prompt_line();
    char *command = readline(prompt);
    free(prompt);
    return command;
}

int readline_init(void)
{
    rl_variable_bind("show-all-if-ambiguous", "on");
    rl_variable_bind("colored-completion-prefix", "on");
    return 0;
}
