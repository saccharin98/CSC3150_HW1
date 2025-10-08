#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[]){
        if (argc < 2) {
                fprintf(stderr, "Usage: %s <test_program> [args...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }

        pid_t pid = fork();
        if (pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
        } else if (pid == 0) {
                char **child_argv = &argv[1];
                execvp(child_argv[0], child_argv);

                if (errno == ENOENT && strchr(child_argv[0], '/') == NULL) {
                        size_t len = strlen(child_argv[0]);
                        char *prefixed = malloc(len + 3);
                        if (prefixed == NULL) {
                                perror("malloc");
                                _exit(EXIT_FAILURE);
                        }
                        prefixed[0] = '.';
                        prefixed[1] = '/';
                        memcpy(prefixed + 2, child_argv[0], len + 1);
                        child_argv[0] = prefixed;
                        execvp(child_argv[0], child_argv);
                }

                perror("execvp");
                _exit(EXIT_FAILURE);
        }

        int status;
        do {
                pid_t w = waitpid(pid, &status, WUNTRACED | WCONTINUED);
                if (w == -1) {
                        perror("waitpid");
                        exit(EXIT_FAILURE);
                }

                if (WIFEXITED(status)) {
                        printf("Child process (%d) terminated normally with exit status %d\n",
                               pid, WEXITSTATUS(status));
                } else if (WIFSIGNALED(status)) {
                        printf("Child process (%d) terminated by signal %d (%s)%s\n",
                               pid,
                               WTERMSIG(status),
                               strsignal(WTERMSIG(status)),
#ifdef WCOREDUMP
                               WCOREDUMP(status) ? " (core dumped)" : "");
#else
                               "");
#endif
                } else if (WIFSTOPPED(status)) {
                        printf("Child process (%d) stopped by signal %d (%s)\n",
                               pid,
                               WSTOPSIG(status),
                               strsignal(WSTOPSIG(status)));
                        if (kill(pid, SIGCONT) == -1) {
                                perror("kill(SIGCONT)");
                                exit(EXIT_FAILURE);
                        }
                } else if (WIFCONTINUED(status)) {
                        printf("Child process (%d) continued\n", pid);
                }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));

        return 0;
}
