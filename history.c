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
    const char *cmd;
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
    /* Ignore invalid command */
    if (strcmp(cmd, "") == 0) {
        return;
    }

    if (count++ >= elist_capacity(history)) {
        elist_remove(history, 0); // super inefficient
    }

    struct hist_entry *hist_elem = malloc(sizeof(struct hist_entry));
    if (hist_elem == NULL) {
        perror("hist_elem malloc");
        return;
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
    fflush(stdout); // clear the buffer
}

/* Retrieves the most recent command starting with 'prefix'
 * or NULL if no match found. */
const char *hist_search_prefix(char *prefix)
{
    int idx = -1;
    for (int i = elist_size(history) - 1; i >= 0; i--) {
        struct hist_entry *hist_elem = elist_get(history, i);
        if (strncmp(hist_elem->cmd, prefix, strlen(prefix)) == 0) {
            idx = i;
            break;
        }
    }

    struct hist_entry *hist_elem = elist_get(history, idx);

    if (hist_elem == NULL) {
        return NULL;
    }

    return hist_elem->cmd;
}

/* Retrieves a particular command number or NULL if no match found */
const char *hist_search_cnum(int command_number)
{
    /* Find the first command number */
    struct hist_entry *first_elem = elist_get(history, 0);
    if (first_elem == NULL) {
        return NULL;
    }

    /* Get proper index */
    int first_cmd = first_elem->cmd_number;
    int idx_num = command_number - first_cmd;
    struct hist_entry *hist_elem = elist_get(history, idx_num);
    if (hist_elem == NULL) {
        return NULL;
    }
    return hist_elem->cmd;
}

/* Retrieve the most recent command number */
unsigned int hist_last_cnum(void)
{
    return count;
}
