#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>

#include "parse.h"

static void singleCommandSubShell(Cmd c);
static void execute_built_in(Cmd c, int proceed);

char *builtin_functions[] = { "cd", "echo", "logout", "nice", "pwd", "setenv","unsetenv", "where" };
static int isBuiltIn(char* func);
static void recursePipe(Cmd c, int in_fd) {
	if (c == NULL) {
		return;
	}
	if ((c->in == Tpipe || c->in == TpipeErr)
			&& (c->out != Tpipe && c->out != TpipeErr)) { //last command in a pipe
		if (!isBuiltIn(c->args[0])) { //last command in pipe not built in
			int pid = fork();
			if (pid == 0) {
				signal(SIGINT, SIG_DFL);
				dup2(in_fd, STDIN_FILENO);
				if (c->out == Tout) {
					int output = open(c->outfile, O_RDWR | O_CREAT,
					S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
					dup2(output, 1);
					close(output);
				} else if (c->out == Tapp) {
					int output = open(c->outfile, O_RDWR | O_CREAT | O_APPEND,
					S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
					dup2(output, 1);
					close(output);
				} else if (c->out == ToutErr) {
					int output = open(c->outfile, O_RDWR | O_CREAT | O_APPEND,
					S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
					dup2(output, 1);
					dup2(output, 2);
					close(output);
				} else if (c->out == TappErr) {
					int output = open(c->outfile, O_RDWR | O_CREAT | O_APPEND,
					S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
					dup2(output, 1);
					dup2(output, 2);
					close(output);
				}
				close(in_fd);
				execvp(c->args[0], c->args);
				perror(c->args[0]);
				exit(EXIT_FAILURE);

			} else {
				int status = -1;
				waitpid(pid, &status, 0);
			}
		} else { //last command in pipe built in
			dup2(in_fd, STDIN_FILENO);
			if (c->out == Tout) {
				int output = open(c->outfile, O_RDWR | O_CREAT,
				S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
				dup2(output, 1);
				close(output);
			} else if (c->out == Tapp) {
				int output = open(c->outfile, O_RDWR | O_CREAT | O_APPEND,
				S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
				dup2(output, 1);
				close(output);
			} else if (c->out == ToutErr) {
				int output = open(c->outfile, O_RDWR | O_CREAT | O_APPEND,
				S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
				dup2(output, 1);
				dup2(output, 2);
				close(output);
			} else if (c->out == TappErr) {
				int output = open(c->outfile, O_RDWR | O_CREAT | O_APPEND,
				S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
				dup2(output, 1);
				dup2(output, 2);
				close(output);
			}
			close(in_fd);
			execute_built_in(c, 1);
		}
	} else { //command in between a pipe
		int fd[2];
		pipe(fd);
		switch (fork()) {
		case -1:
		case 0:
			signal(SIGINT, SIG_DFL);
			close(fd[0]);
			dup2(in_fd, STDIN_FILENO);
			dup2(fd[1], STDOUT_FILENO);
			if (c->out == TpipeErr) {

				dup2(fd[1], STDERR_FILENO);
			}
			close(fd[1]);
			if (!isBuiltIn(c->args[0])) {
				execvp(c->args[0], c->args);
				perror(c->args[0]);
				exit(EXIT_FAILURE);
			} else {
				execute_built_in(c, 0);
				exit(EXIT_SUCCESS);
			}
		default:
			close(fd[1]);
			recursePipe(c->next, fd[0]);
		}
	}
}
static int isBuiltIn(char* func) {
	int builtin = 0;
	int num_of_builtins = sizeof(builtin_functions)
			/ sizeof(builtin_functions[0]);
	int i = 0;
	for (i = 0; i < num_of_builtins; i++) {
		if (strcmp(func, builtin_functions[i]) == 0) {
			builtin = 1;
			break;
		}
	}
	return builtin;
}
static int isNumber(char* str) {
	if (!isdigit(*str) && *str != '-') {
		return 0;
	}
	str++;
	while (*str) {
		if (!isdigit(*str)) {
			return 0;
		}
		str++;
	}
	return 1;
}
static void execute_built_in(Cmd c, int proceed) {
	if (strcmp(c->args[0], "cd") == 0) {
		if ((c->args[1]) == NULL) {
			chdir("/");
		} else {
			if (chdir(c->args[1]) == -1) {
				printf("ush: cd: %s: No such file or directory\n", c->args[1]);
			}
		}

	} else if (strcmp(c->args[0], "echo") == 0) {
		int i = 1;
		int valid = 1;
		char* invalid;
		char* word = c->args[i];
		while (word) {
			if (word[0] == '$' && (word + 1) != NULL) {
				if (getenv(word + 1) == NULL) {
					valid = 0;
					invalid = word + 1;
					break;
				}
			}
			word = c->args[++i];
		}
		if (!valid) {
			printf("%s: Undefined variable", invalid);
		} else {
			i = 1;
			word = c->args[i];
			while (word) {
				if (word[0] == '$' && (word + 1) != NULL) {
					printf("%s ", getenv(word + 1));
				} else if (word[0] == '$' && (word + 1) == NULL) {
					printf("$ ");
				} else {
					printf("%s ", word);
				}
				word = c->args[++i];
			}
		}

		if (c->args[1] != NULL) {
			printf("\n");
		}
	} else if (strcmp(c->args[0], "logout") == 0) {
		exit(EXIT_SUCCESS);
	} else if (strcmp(c->args[0], "pwd") == 0) {
		char path[1024];
		getcwd(path, sizeof(path));
		printf("%s\n", path);
	} else if (strcmp(c->args[0], "nice") == 0) {
		int which = PRIO_PROCESS;
		id_t pid;
		long priority = 4;
		if (c->nargs == 1) {
			pid = getpid();
			setpriority(which, pid, priority);
		} else if (c->nargs == 2) {
			if (isNumber(c->args[1])) {
				priority = strtol(c->args[1], NULL, 10);
				if (priority > 19) {
					priority = 19;
				} else if (priority < -20) {
					priority = -20;
				}
				pid = getpid();
				setpriority(which, pid, priority);
			} else {
				int status;
				pid = fork();
				switch (pid) {
				case 0:
					signal(SIGINT, SIG_DFL);
					setpriority(which, pid, priority);
					execvp(c->args[1], c->args + 1);
					perror(c->args[1]);
					exit(EXIT_FAILURE);
				case -1:
					perror("fork unsuccessful\n");
					break;
				default:
					waitpid(pid, &status, 0);
				}
			}
		} else { //arguments more than 2
			if (isNumber(c->args[1])) {
				int status;
				priority = strtol(c->args[1], NULL, 10);
				if (priority > 19) {
					priority = 19;
				} else if (priority < -20) {
					priority = -20;
				}
				pid = fork();
				switch (pid) {
				case 0:
					signal(SIGINT, SIG_DFL);
					setpriority(which, pid, priority);
					execvp(c->args[2], c->args + 2);
					perror(c->args[2]);
					exit(EXIT_FAILURE);
				case -1:
					perror("fork unsuccessful\n");
					break;
				default:
					waitpid(pid, &status, 0);
				}
			} else {
				int status;
				pid = fork();
				switch (pid) {
				case 0:
					signal(SIGINT, SIG_DFL);
					setpriority(which, pid, priority);
					execvp(c->args[1], c->args + 1);
					perror(c->args[1]);
					exit(EXIT_FAILURE);
				case -1:
					perror("fork unsuccessful\n");
					break;
				default:
					waitpid(pid, &status, 0);
				}
			}
		}
		if (c->args[1] != NULL) {
			priority = strtol(c->args[1], NULL, 10);
			if (priority > 19) {
				priority = 19;
			} else if (priority < -20) {
				priority = -20;
			}
		}
		if (priority == 0 && !strcmp("0", c->args[1])) {
			priority = 4;
		}

	} else if (strcmp(c->args[0], "setenv") == 0) {
		if (c->args[1] == NULL) {
			extern char **environ;
			int i;
			for (i = 0; environ[i] != NULL; i++) {
				printf("%s\n", environ[i]);
			}
		} else {
			char *argument;
			argument = (char *) malloc(
					strlen(c->args[1]) + strlen(c->args[2]) + 2);
			strcpy(argument, c->args[1]);
			strcat(argument, "=");
			strcat(argument, c->args[2]);
			putenv(argument);

		}
	} else if (strcmp(c->args[0], "unsetenv") == 0) {
		extern char **environ;
		char **ep, **sp;
		size_t len;

		if (c->args[1] == NULL
				|| c->args[1][0] == '\0'|| strchr(c->args[1], '=') != NULL) {
			return;
		}

		len = strlen(c->args[1]);

		for (ep = environ; *ep != NULL;)
			if (strncmp(*ep, c->args[1], len) == 0 && (*ep)[len] == '=') {

				for (sp = ep; *sp != NULL; sp++)
					*sp = *(sp + 1);

			} else {
				ep++;
			}

	} else if (strcmp(c->args[0], "where") == 0) {
		if (c->args[1] != NULL) {
			printf("%s:", c->args[1]);
			if (isBuiltIn(c->args[1])) {
				printf(" shell built-in command\n");
			}
			char* path = getenv("PATH");
			char* pathCpy = strdup(path);
			char* token = strtok(pathCpy, ":");
			char* tokenCpy = strdup(token);
			strcat(tokenCpy, "/");
			strcat(tokenCpy, c->args[1]);
			if (access(tokenCpy, F_OK) != -1) {
				printf(" %s", tokenCpy);
			} else {
			}
			while ((token = strtok(NULL, ":")) != NULL) {
				tokenCpy = strdup(token);
				strcat(tokenCpy, "/");
				strcat(tokenCpy, c->args[1]);
				if (access(tokenCpy, F_OK) != -1) {
					printf(" %s", tokenCpy);
				} else {
				}
			}
			printf("\n");
		}
	}

	fflush(stdout);
	fflush(stderr);
	if (proceed) {
		singleCommandSubShell(c->next);
	}
}
static void singleCommandSubShell(Cmd c) {
	if (c == NULL) {
		return;
	}
	int input = 0, output = 0, in = 0, out = 0, err = 0, outpipe = 0,
			outpipeerr = 0, built_in = 0;
	;
	built_in = isBuiltIn(c->args[0]);
	if (c->in == Tin) {
		input = open(c->infile, O_RDONLY);
		if (input == -1) {
			printf("ush: %s : %s: No such file or directory\n", c->args[0],
					c->infile);
			fflush(stdin);
		} else {
			in = 1;
		}
	}
	if (c->out != Tnil)
		switch (c->out) {
		case Tout:
			output = open(c->outfile, O_RDWR | O_CREAT,
			S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
			out = 1;
			break;
		case Tapp:
			output = open(c->outfile, O_RDWR | O_CREAT | O_APPEND,
			S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
			out = 1;
			break;
		case ToutErr:
			output = open(c->outfile, O_RDWR | O_CREAT,
			S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
			out = 1;
			err = 1;
			break;
		case TappErr:
			output = open(c->outfile, O_RDWR | O_CREAT | O_APPEND,
			S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
			out = 1;
			err = 1;
			break;
		case Tpipe:
			outpipe = 1;
			break;
		case TpipeErr:
			outpipeerr = 1;
			break;
		default:
			fprintf(stderr, "Shouldn't get here\n");
			exit(-1);
		}

	if (outpipe || outpipeerr) { //piped command
		int fd[2];
		pipe(fd);
		switch (fork()) {
		case -1:
		case 0:
			close(fd[0]);
			dup2(input, STDIN_FILENO);
			dup2(fd[1], STDOUT_FILENO);
			if (outpipeerr) {
				dup2(fd[1], STDERR_FILENO);
			}
			close(input);
			close(fd[1]);
			if (built_in) { //piped and built in
				execute_built_in(c, 0);
				exit(EXIT_SUCCESS);
			} else { //piped and not built in
				execvp(c->args[0], c->args);
				perror(c->args[0]);
				exit(EXIT_FAILURE);
			}
			break;
		default:
			close(fd[1]);
			recursePipe(c->next, fd[0]);
		}
	} else { //not piped
		pid_t pid;
		int status;
		if (!built_in) { //not piped and not built-in
			pid = fork();
			if (pid == 0) {
				signal(SIGINT, SIG_DFL);
				if (c->exec == Tamp) {
					setpgid(0, 0);
				}
				if (in) {
					dup2(input, 0);
					close(input);
				}
				if (out) {
					dup2(output, 1);
					close(output);
				}
				if (err) {
					dup2(output, 2);
					close(output);
				}
				execvp(c->args[0], c->args);
				perror(c->args[0]);
				exit(EXIT_FAILURE);
			} else if (pid < 0) {
				perror("error forking");
			} else {
				if (c->exec == Tamp) {
					waitpid(pid, &status, WNOHANG);
				} else {
					waitpid(pid, &status, 0);
				}

				singleCommandSubShell(c->next);
				return;
			}
		} else { //not piped but built-in
			if (in) {
				dup2(input, 0);
				close(input);
			}
			if (out) {
				dup2(output, 1);
				close(output);
			}
			if (err) {
				dup2(output, 2);
				close(output);
			}
			execute_built_in(c, 1);
		}
	}
}
static void execute(Pipe p) {
	if (p == NULL)
		return;
	Cmd c = p->head;
	if (!strcmp(c->args[0], "end")) {
		exit(0);
	}
	singleCommandSubShell(c);

	execute(p->next);
}
int main(int argc, char *argv[]) {

	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);

	Pipe p;
	char path[1024];
	char hostname[1024];
	char user[1024];
	int stdincopy = dup(0);
	int stdoutcopy = dup(1);
	int stderrcopy = dup(2);

	char* ushrcloc = getenv("HOME");
	strcat(ushrcloc, "/.ushrc");

	if (access(ushrcloc, R_OK) != -1) {
		int f = open(ushrcloc, O_RDONLY);
		dup2(f, STDIN_FILENO);
		close(f);
		p = parse();
		execute(p);
		freePipe(p);
	}
	while (1) {
		dup2(stdincopy, 0);
		dup2(stdoutcopy, 1);
		dup2(stderrcopy, 2);

		getcwd(path, sizeof(path));
		gethostname(hostname, 1024);
		getlogin_r(user, 1024);
		if (isatty(STDIN_FILENO)) {
			printf("%s@%s:%s%% ", user, hostname, path);
			fflush(stdout);
			fflush(stderr);
		}
		p = parse();
		execute(p);
		freePipe(p);
	}
}


