/**
 * @file
 *
 * Text-based UI functionality. These functions are primarily concerned with
 * interacting with the readline library.
 */

#ifndef _UI_H_
#define _UI_H_

void init_ui(void);
char *prompt_line(void);
char *prompt_username(void);
char *prompt_hostname(void);
char *get_home(void);
char *prompt_cwd(void);
int get_prompt_status(void);
void set_prompt_status(int val);
unsigned int prompt_cmd_num(void);
char *read_command(void);

#endif
