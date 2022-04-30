#include <stdio.h>
#include <readline/readline.h>
#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <limits.h>

#include "history.h"
#include "logger.h"
#include "ui.h"

static const char *good_str = "😋";
static const char *bad_str  = "😭";
static bool scripting = false;
static int prompt_status;
static char *user;
static char host[HOST_NAME_MAX + 1];
// + 1 to deal with possible truncation in gethostname()

static int readline_init(void);

void init_ui(void)
{
    LOGP("Initializing UI...\n");

    char *locale = setlocale(LC_ALL, "en_US.UTF-8");
    LOG("Setting locale: %s\n",
            (locale != NULL) ? locale : "could not set locale!");
    
    user = prompt_username();
    strcpy(host, prompt_hostname());

    rl_startup_hook = readline_init;

    if (!isatty(STDIN_FILENO)) {
        LOGP("Data piped in on stdin; entering script mode\n");
        scripting = true;
    }
}

char *prompt_line(void)
{
    const char *status = get_prompt_status() ? bad_str : good_str;

    char cmd_num[25];
    snprintf(cmd_num, 25, "%u", prompt_cmd_num());

    //char *user = prompt_username();
    //char *host = prompt_hostname();
    char *cwd = prompt_cwd();

    char *format_str = "[%s]-[%s]-[%s@%s:%s]$ ";

    size_t prompt_sz
        = strlen(format_str)
        + strlen(status)
        + strlen(cmd_num)
        + strlen(user)
        + strlen(host)
        + strlen(cwd)
        + 1;

    char *prompt_str = malloc(sizeof(char) * prompt_sz);

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
    uid_t uid = getuid();
    /* Password struct exists in memory when our program
     * runs so not actually allocating memory here */
    struct passwd *pwd = getpwuid(uid);
    return pwd->pw_name;        
}

char *prompt_hostname(void)
{
    gethostname(host, HOST_NAME_MAX);
    return host;
}

char *get_home(void) {
    char *home_dir = malloc(strlen("/home/") + strlen(user) + 1);
    if (home_dir == NULL) {
        perror("home_dir malloc error");
        return NULL;
    }
    memcpy(home_dir, "/home/", strlen("/home/"));
    strcat(home_dir, user);

    return home_dir;
}

char *prompt_cwd(void)
{
    char *cwd = malloc(PATH_MAX);
    if (cwd == NULL) {
        perror("cwd malloc error");
    }
    cwd = getcwd(cwd, PATH_MAX);

    char *home_dir = get_home();
    if (strncmp(cwd, home_dir, strlen(home_dir)) == 0) {
        *(cwd + strlen(home_dir) - 1) = '~';
        cwd = cwd + strlen(home_dir) - 1;
    }

    free(home_dir);
    return cwd;
}

int get_prompt_status(void)
{
    return prompt_status;
}

void set_prompt_status(int val) {
    prompt_status = val;
}

unsigned int prompt_cmd_num(void)
{
    return hist_last_cnum() + 1;
}

char *read_command(void)
{
    if (scripting) {
        char *line = NULL;
        size_t buf_sz = 0;
        ssize_t read_sz = getline(&line, &buf_sz, stdin);
        if (read_sz == -1) {
            free(line);
            return NULL;
        }
        line[read_sz - 1] = '\0';
        return line;
    } else {
        char *prompt = prompt_line();
        /* Allows us to arrow back and forth over the line we are typing */
        char *command = readline(prompt);
        free(prompt);
        return command;
    }
}

int readline_init(void)
{
    rl_variable_bind("show-all-if-ambiguous", "on");
    rl_variable_bind("colored-completion-prefix", "on");
    return 0;
}
