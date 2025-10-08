#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>

static void report_signal(int signo)
{
	switch (signo) {
	case 6:
		printf("child process get SIGABRT signal\n");
		break;
	case 14:
		printf("child process get SIGALRM signal\n");
		break;
	case 7:
		printf("child process get SIGBUS signal\n");
		break;
	case 8:
		printf("child process get SIGFPE signal\n");
		break;
	case 1:
		printf("child process get SIGHUP signal\n");
		break;
	case 4:
		printf("child process get SIGILL signal\n");
		break;
	case 2:
		printf("child process get SIGINT signal\n");
		break;
	case 9:
		printf("child process get SIGKILL signal\n");
		break;
	case 13:
		printf("child process get SIGPIPE signal\n");
		break;
	case 3:
		printf("child process get SIGQUIT signal\n");
		break;
	case 11:
		printf("child process get SIGSEGV signal\n");
		break;
	case 5:
		printf("child process get SIGTRAP signal\n");
		break;
	case 15:
		printf("child process get SIGTERM signal\n");
		break;
	default:
		break;
	}
}

static void run_child(int argc, char *argv[])
{
	int idx;
	char *child_argv[argc];

	for (idx = 1; idx < argc; ++idx)
		child_argv[idx - 1] = argv[idx];
	child_argv[argc - 1] = NULL;

	printf("I'm the Child Process, my pid = %d\nChild process start to execute test program:\n", getpid());
	execve(child_argv[0], child_argv, NULL);
	_exit(EXIT_SUCCESS);
}

static void handle_parent(pid_t child_pid)
{
	int status;

	printf("I'm the Parent Process, my pid = %d\n", getpid());
	waitpid(child_pid, &status, WUNTRACED);
	printf("Parent process receives SIGCHLD signal\n");

	if (WIFEXITED(status)) {
		printf("Normal termination with EXIT STATUS = %d\n", WEXITSTATUS(status));
	} else if (WIFSTOPPED(status)) {
		printf("child process get SIGSTOP signal\n");
	} else {
		report_signal(status & 127);
	}

	exit(0);
}

int main(int argc, char *argv[])
{
	pid_t pid;

	printf("Process start to fork\n");
	pid = fork();

	if (pid < 0) {
		perror("fork");
		exit(1);
	}

	if (pid == 0)
		run_child(argc, argv);
	else
		handle_parent(pid);

	return 0;
}