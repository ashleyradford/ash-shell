#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "logger.h"
#include "history.h"
#include "elist.h"

static struct elist *history;
static int count = 0;
struct hist_entry {
    char *cmd;
    int cmd_number;
};

void hist_init(unsigned int limit)
{
    /* Create history elist */
    history = elist_create(limit);
}

void hist_destroy(void)
{
    /* Destory history elist */
    elist_destroy(history);
}

void hist_add(const char *cmd)
{
    if (strcmp(cmd, "") == 0) {
        return;
    }

    if (count++ >= elist_capacity(history)) {
        elist_remove(history, 0); // super inefficient
    }

    struct hist_entry *hist_elem = malloc(sizeof(struct hist_entry));
    if (hist_elem == NULL) {
        perror("hist_elem malloc");
    }

    hist_elem->cmd = strdup(cmd);
    hist_elem->cmd_number = count;
    elist_add(history, hist_elem);
}

void hist_print(void)
{
    for (int i = 0; i < elist_size(history); i++) {
        struct hist_entry *hist_elem = elist_get(history, i);
        printf("%d %s\n", hist_elem->cmd_number, hist_elem->cmd);
    }
}

const char *hist_search_prefix(char *prefix)
{
    // TODO: Retrieves the most recent command starting with 'prefix', or NULL
    // if no match found.
    return NULL;
}

const char *hist_search_cnum(int command_number)
{
    // TODO: Retrieves a particular command number. Return NULL if no match
    // found.
    return NULL;
}

unsigned int hist_last_cnum(void)
{
    // TODO: Retrieve the most recent command number.
    return 0;
}
