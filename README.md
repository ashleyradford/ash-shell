# Project 3: Command Line Shell

See: https://www.cs.usfca.edu/~mmalensek/cs521/assignments/project-3.html

TODO: Remove the link above. Your README should not depend on a link to the spec.

TODO: Replace this section with a short (1-3 paragraph) description of the project. What it does, how it does it, and any features that stand out. If you ever need to refer back to this project, the description should jog your memory.

## Building

To compile and run:
```bash
make     # compiles ash shell
./ash    # non scripting mode
```

To run in scriting mode:
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
[😋]-[1]-[aeradford@astra:~/P3-ashleyradford]$ cat ui.c | grep bind | tr [:lower:] [:upper:]
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
1 cat ui.c | grep bind | tr [:lower:] [:upper:]
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
