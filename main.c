/* Daniel Funke
   CSC 345-01
   Project 1 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>


/* Definition of global variables */
#define SHELL_PROMPT "osc> "
#define SHELL_TOKEN_BUFSIZE 64
#define SHELL_TOKEN_DELIM " \t\r\n\a"
#define MAX_LINE_LENGTH 80
#define HISTORY_LENGTH 10

/* Function Stubs */
char **shell_split_line(char *line);

/* this function parses through args[] to check for
   redirect chars and returns an integer representing
   index position if redirect char is found, otherwise
   returns 0 */
int shell_redirect_check(char **args)
{
	int i = 1;
	int in, out;
	while (args[i] != NULL) {
		in = strcmp(args[i], "<");
		out = strcmp(args[i], ">");
		if (in == 0 || out == 0) {
			return i;
		} else {
			++i;
		}
	}

	return 0;
}

/* this function parses through args[] to check for
   pipe char and returns an integer representing the
   index position if a pipe char is found, otherwise
   returns 0 */
int shell_pipe_check(char **args)
{
	int i = 1;
	int cmp;
	while (args[i] != NULL) {
		cmp = strcmp(args[i], "|");
		if (cmp == 0) {
			return i;
		} else {
			++i;
		}
	}

	return 0;
}

/* this function checks the last element of args[]
   for the ampersand char. If last argument is '&'
   it is replaced with a null char. returns 0 if
   last char is '&', otherwise returns nonzero int */
int shell_bg_check(char **args)
{
	int i = 0;
	int cmp;
	char *last_arg;

	while (args[i] != NULL) {
		++i;
	}
	last_arg = args[i - 1];
	cmp = strcmp(last_arg, "&");

	if (cmp == 0) {
		args[i - 1] = NULL;
	}

	return cmp;
}

/* this function executes the redirect operation. input
   parameter index defines element in args[] where redirect
   char is. argument following redirect char describes
   file descriptor is saved as file_desc. returns zero if
   exec is succesful, otherwise returns error */
int launch_redirect(char **args, int index)
{
	int in, out, file_desc;
	char *fd;

	in = strcmp(args[index], "<");
	out = strcmp(args[index], ">");
	fd = args[index + 1];
	args[index] = NULL; // terminate args[] at '>' or '<'

	if (in == 0) {
		// open read only file if redirect input from file
		if ((file_desc = open(fd, O_RDONLY, 0644)) < 0) {
			perror("shell");
			exit(1);
		}
		dup2(file_desc, STDIN_FILENO);
		close(file_desc);
	} else if (out == 0) {
		// create or open writable file if redirect to output
		if ((file_desc = open(fd, O_CREAT | O_WRONLY | O_TRUNC, 0644)) < 0) {
			perror("shell");
			exit(1);
		}
		dup2(file_desc, STDOUT_FILENO);
		close(file_desc);
	}
	// execute command
	if (execvp(args[0], args) == -1) {
		perror("exec");
	}

	return 0;
}

/* this function executes the pipe operation. Returns 0 if
   pipe operation is successful, otherwise returns an error */
int launch_pipe(char *line)
{
	int fd[2];
	char *cmds[2];
	char **ch_args, **pa_args;
	int i = 0;

	/* separate input line into two separate strings, then
	   convert strings into arg arrays */
	while ((cmds[i] = strsep(&line, "|")) != NULL) {
		++i;
	}
	pa_args = shell_split_line(cmds[0]); // parent process args
	ch_args = shell_split_line(cmds[1]); // child process args

	/* create pipe for interprocess communication */
	pipe(fd);

	/* create child process */
	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(1);
	}

	if (pid == 0) { // child process
		// connect standard input to the read end of the pipe
		dup2(fd[0], 0);

		// close the write end of the pipe for the child
		close(fd[1]);

		// execute child commands
		execvp(ch_args[0], ch_args);

		perror("exec");
		exit(1);
	} else { //parent process
		// connect standard output to the write end of the pipe
		dup2(fd[1], 1);

		// close the read end of the pipe for the parent
		close(fd[0]);

		// execute parent command
		if (execvp(pa_args[0], pa_args) == -1){
			perror("exec");
			exit(1);
		}
	}

	return 0;
}

/* this function creates a child process to execute user
   commands. calls redirect, pipe, and bg check functions
   to determine appropriate execution. returns 1 if 
   successful, otherwise returns error */
int shell_launch(char *line, char **args)
{
	pid_t pid, wpid;
	int status;
	int redir = shell_redirect_check(args);
	int pipev = shell_pipe_check(args);
	int bg = shell_bg_check(args);

	/* create child process */
	pid = fork();
	if (pid < 0) { // error forking
		perror("fork");
		exit(1);
	} else if (pid == 0) { // child process
		// redirect output or input if requested by user
		if (redir != 0) {
			launch_redirect(args, redir);	
		} else if (pipev != 0) {	
			launch_pipe(line);
		} else {
			if (execvp(args[0], args) == -1) {
				perror("exec");
			}
		}
	} else { // parent process
		/* if '&' is last char in arg[], do not wait for child
		   process to finish before continuing with shell process */
		if (bg != 0) {
			do {
				wpid = waitpid(pid, &status, WUNTRACED);
			} while (!WIFEXITED(status) && !WIFSIGNALED(status));
		}
	}

	return 1;
}

/* Built-In Shell Commands */
char *builtin_list[] = {"cd", "exit"};

int num_builtins() {
	return (sizeof(builtin_list) / sizeof(char *));
}

/* change directory function. returns 1 if successful, otherwise
   returns an error */
int shell_cd (char **args)
{
	if (args[1] == NULL) {
		fprintf(stderr, "shell: expected argument to \"cd\"\n");
	} else {
		if (chdir(args[1]) != 0) {
			perror("shell");
		}
	}

	return 1;
}

/* exit shell function. always returns zero when called */
int shell_exit(char **args)
{
	return 0;
}

/* history function. prints previous entered commands to
   stdout. number of historical commands determined by
   global variable HISTORY_LENGTH */
int shell_hist(char *hist_buf[], int current_cmd)
{
	int i = current_cmd;
	int hist_num = 1;

	do {
		if(hist_buf[i]) {
			printf("%d) %s", hist_num, hist_buf[i]);
			hist_num++;
		}

		i = (i + 1) % HISTORY_LENGTH;
	} while (i != current_cmd);

	return 1;
}

/* array of addresses for built-in commands */
int (*builtin_cmds[]) (char **) = {
	&shell_cd,
	&shell_exit
};

/* this function pulls last executed command from history
   buffer and assigns its values to the current args[] array.
   returns new args[] array if there exists a cmd to repeat 
   otherwise returns NULL */
char **shell_repeat_last(char *hist_buf[], int *current_cmd)
{
	int index = *current_cmd;
	char *line;	
	
	/* check to see if this is the first command entered */
	if (index > 1) {
		line = strdup(hist_buf[index - 2]);
		// print error if previous cmd was also repeat
		if (strcmp(line, "!!\n") == 0) {
			printf("No commands in history\n");
			return (NULL);
		}
		printf("%s", line);	
		hist_buf[index - 1] = strdup(line);
		return shell_split_line(line);
	} else {
		// if first command entered, print error to stdout
		printf("No commands in history\n");	
		return(NULL);
	}
}

/* this command calls checks input arguments for built-in
   commands and executes them if found, else calls the
   shell_launch function to execute normal shell commands */
int shell_execute(char **args, char *hist_buf[], int *current_cmd)
{	
	char **temp;

	if (args[0] == NULL) {
		return 1;
	}

	// check for and execute if user entered repeat cmd
	if (strcmp(args[0], "!!") == 0) {
		temp = (shell_repeat_last(hist_buf, current_cmd));
		if (temp != NULL) {
			args = temp;
		} else {
			return 1;
		}
	}

	// check for and execute if user entered built-in cmd
	for (int i = 0; i < num_builtins(); ++i) {
		if (strcmp(args[0], builtin_list[i]) == 0) {
			return (*builtin_cmds[i])(args);
		}
	}

	// check for and execute if user entered hist cmd
	if (strcmp(args[0], "hist") == 0) {
		return (shell_hist(hist_buf, *current_cmd));
	}

	char *line = hist_buf[*current_cmd - 1];
	return shell_launch(line, args);
}

/* this function takes a string as input and returns an
   array of tokens corresponding to individual elements of
   the input string */
char **shell_split_line(char *line)
{
	int bufsize = SHELL_TOKEN_BUFSIZE;
	int index = 0;
	char **tokens = malloc(bufsize * sizeof(char*));
	char *token;

	/* Print error to screen if unable to allocate memory */
	if (!tokens) {
		fprintf(stderr, "shell: mem allocation error\n");
		exit(EXIT_FAILURE);
	}

	/* split input line into individual string tokens */
	token = strtok(line, SHELL_TOKEN_DELIM);
	while (token != NULL) {
		tokens[index] = token;
		++index;

		/* double buffer size if index goes out of bounds */
		if (index >= bufsize) {
			bufsize = bufsize + SHELL_TOKEN_BUFSIZE;
			tokens = realloc(tokens, bufsize * sizeof(char*));
			if (!tokens) {
				fprintf(stderr, "shell: mem allocation error\n");
				exit(EXIT_FAILURE);
			}
		}

		token = strtok(NULL, SHELL_TOKEN_DELIM);
	}

	tokens[index] = NULL;
	return tokens;
}

/* this function gets input from the user */
char *shell_read_line(void)
{
	char *line = NULL;
	ssize_t buffer_size = 0;
	getline(&line, &buffer_size, stdin);
	return line;
}

/* this is the main loop of the shell program. it creates an
   empty history buffer at start, then performs a conditional
   loop dependent upon the status variable. the loop handles
   adding elements to history buffer and calls shell_execute 
   to execute user cmds */
void shell_main_loop(void)
{
	char *line;
	char **args;
	char *hist_buf[HISTORY_LENGTH];
	int status;
	int current_cmd = 0;	

	/* Initialize History Buffer */
	for (int i = 0; i < HISTORY_LENGTH; ++i) {
		hist_buf[i] = NULL;
	}

	do {
		/* Print shell prompt to screen and get input
		   from user. Store user input in line array */
		printf(SHELL_PROMPT);
		line = shell_read_line();

		/* Add user input to most recent slot in hist_buf */
		free(hist_buf[current_cmd]);
		hist_buf[current_cmd] = strdup(line);
		current_cmd = (current_cmd + 1) % HISTORY_LENGTH;

		/* Parse input line into individual tokens */
		args = shell_split_line(line);

		status = shell_execute(args, hist_buf, &current_cmd);
		free(line);
		free(args);

	} while(status);
}


int main()
{
	shell_main_loop();
	return EXIT_SUCCESS;
}
