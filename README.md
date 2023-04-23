# Command Line Shell: `ash`

A command line interface that responds to user input. The prompt of ash displays the status of the most recent command, the current command number, username and hostname, and the current working directory. Scripting mode is also supported by ash where commands are read from standard input and executed without displaying the prompt.

This shell supports the following built-in commands:

* `cd` will change the current working directory, cd without arguments will return to the user's home directory
* `# (comments)` all strings prefixed with # will be ignored
* `history` prints the last 100 commands entered with their command numbers
* `!(history execution)` entering !39 will re-run command number 39 and !! reruns the last command that was entered, !ls re-runs the last command that starts with `ls`
* `exit` will exit ash

Commands from the user are first tokenized and added to an elist that can be dynamically resized as needed. From this list of tokens, commands are separated and added to a commands elist - each command from a user is separated by a pipe. Once this is done, the commands elist is sent to a process handler.

This handler forks a child process where the commands are sent into `execute_pipeline()`. Here, the commands are executed - if there is a single command a single `execvp()` is called, otherwise `execute_pipeline()` forks a new child process for each additional command. These commands are able to communicate with each other through `pipe()`. Redirection is also supported here by making use of the `dup2()` system call.

## Building

To compile and run:
```bash
make     # compiles ash shell
./ash    # non scripting mode
```

To run in scripting mode:
```bash
./ash < [some_input_file]
```

## Included Files

* **elist.c** -- library that implements a dynamic array
* **elist.h** -- header file for elist
* **history.c** -- sets up shell history data structures and retrieval functions
* **history.h** -- header file for history
* **logger.h** -- provides basic logging functionality
* **shell.c** -- command line interface for ash shell
* **ui.c** -- provides text based UI functionality
* **ui.h** -- header file for ui

## Testing

To execute the test cases, use `make test`. To pull in updated test cases, run `make testupdate`. You can also run a specific test case instead of all of them:

```
# Run all test cases:
make test

# Run a specific test case:
make test run=4

# Run a few specific test cases (4, 8, and 12 in this case):
make test run='4 8 12'

# Run a test case in gdb:
make test run=4 debug=on
```

If you are satisfied with the state of your program, you can also run the test cases on the grading machine. Check your changes into your project repository and then run:

```
make grade
```

## Demo Run

```
[aeradford@astra P3-ashleyradford]$ ./ash
[😋]-[1]-[aeradford@astra:~/P3-ashleyradford]$ grep bind < ui.c | tr [:lower:] [:upper:]
    RL_VARIABLE_BIND("SHOW-ALL-IF-AMBIGUOUS", "ON");
    RL_VARIABLE_BIND("COLORED-COMPLETION-PREFIX", "ON");
[😋]-[2]-[aeradford@astra:~/P3-ashleyradford]$ hallo
Bad command: No such file or directory
[😭]-[3]-[aeradford@astra:~/P3-ashleyradford]$ cd tests/inputs/
[😋]-[4]-[aeradford@astra:~/P3-ashleyradford/tests/inputs]$ ls | sed s|v|3|g;s|a|<|g; | grep <3
n<3ig<tion1.sh
n<3ig<tion2.sh
[😋]-[5]-[aeradford@astra:~/P3-ashleyradford/tests/inputs]$ cd ..
[😋]-[6]-[aeradford@astra:~/P3-ashleyradford/tests]$ history
1 grep bind < ui.c | tr [:lower:] [:upper:]
2 hallo
3 cd tests/inputs/
4 ls | sed s|v|3|g;s|a|<|g; | grep <3
5 cd ..
6 history
[😋]-[7]-[aeradford@astra:~/P3-ashleyradford/tests]$ !2
Bad command: No such file or directory
[😭]-[8]-[aeradford@astra:~/P3-ashleyradford/tests]$ echo yay
yay
[😋]-[9]-[aeradford@astra:~/P3-ashleyradford/tests]$ exit
```
