#include <linux/module.h>
#include <linux/atomic.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/kthread.h>
#include <linux/pid.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/signal.h>

MODULE_LICENSE("GPL");

#define TEST_PATH "/tmp/test"

static struct task_struct *worker;
static atomic_t child_pid = ATOMIC_INIT(-1);

struct child_info {
        pid_t pid;
};

static const char *signal_to_name(int sig)
{
        switch (sig) {
        case SIGHUP:  return "SIGHUP";
        case SIGINT:  return "SIGINT";
        case SIGQUIT: return "SIGQUIT";
        case SIGILL:  return "SIGILL";
        case SIGTRAP: return "SIGTRAP";
        case SIGABRT: return "SIGABRT";
        case SIGBUS:  return "SIGBUS";
        case SIGFPE:  return "SIGFPE";
        case SIGKILL: return "SIGKILL";
        case SIGUSR1: return "SIGUSR1";
        case SIGSEGV: return "SIGSEGV";
        case SIGUSR2: return "SIGUSR2";
        case SIGPIPE: return "SIGPIPE";
        case SIGALRM: return "SIGALRM";
        case SIGTERM: return "SIGTERM";
        case SIGCHLD: return "SIGCHLD";
        case SIGCONT: return "SIGCONT";
        case SIGSTOP: return "SIGSTOP";
        case SIGTSTP: return "SIGTSTP";
        case SIGTTIN: return "SIGTTIN";
        case SIGTTOU: return "SIGTTOU";
        default:      return "UNKNOWN";
        }
}

static void reset_sigaction(struct task_struct *task)
{
        int i;
        struct k_sigaction *ka;

        if (!task->sighand)
                return;

        ka = &task->sighand->action[0];
        for (i = 0; i < _NSIG; i++, ka++) {
                ka->sa.sa_handler = SIG_DFL;
                ka->sa.sa_flags = 0;
                ka->sa.sa_restorer = NULL;
                sigemptyset(&ka->sa.sa_mask);
        }
}

static void translate_wait(int status, bool *exited, int *exit_code,
                           bool *signaled, int *term_sig,
                           bool *stopped, int *stop_sig)
{
        int sig = status & 0x7f;

        *exited = *signaled = *stopped = false;
        *exit_code = *term_sig = *stop_sig = 0;

        if (sig == 0) {
                *exited = true;
                *exit_code = (status >> 8) & 0xff;
                return;
        }

        if (sig == 0x7f) {
                *stopped = true;
                *stop_sig = (status >> 8) & 0xff;
                return;
        }

        *signaled = true;
        *term_sig = sig;
}

static int helper_init(struct subprocess_info *info, struct cred *new)
{
        struct child_info *data = info->data;
        pid_t pid = task_pid_nr(current);

        if (data)
                data->pid = pid;

        atomic_set(&child_pid, pid);
        return 0;
}


static int run_user_process(void *unused)
{
        char *argv[] = { (char *)TEST_PATH, NULL };
        char *envp[] = {
                "HOME=/",
                "PATH=/sbin:/bin:/usr/sbin:/usr/bin",
                NULL,
        };
        struct subprocess_info *info;
        struct child_info ctx;
        int ret;
        int status;
        bool exited, signaled, stopped;
        int exit_code, term_sig, stop_sig;
        pid_t child;

        pr_info("[program2] : module_init kthread start\n");

        ctx.pid = -1;
        atomic_set(&child_pid, -1);

        reset_sigaction(current);

        info = call_usermodehelper_setup(argv[0], argv, envp,
                                         GFP_KERNEL,
                                         helper_init,
                                         NULL,
                                         &ctx);
        if (!info) {
                pr_err("[program2] : failed to setup usermodehelper\n");
                return -ENOMEM;
        }

        ret = call_usermodehelper_exec(info, UMH_WAIT_PROC);

        child = ctx.pid;
        atomic_set(&child_pid, -1);

        pr_info("[program2] : The child process has pid = %d\n", child);
        pr_info("[program2] : This is the parent process, pid = %d\n", current->pid);
        pr_info("[program2] : child process\n");

        if (ret < 0) {
                pr_err("[program2] : exec failed, rc = %d\n", ret);
                return ret;
        }

        status = ret;
        translate_wait(status, &exited, &exit_code, &signaled, &term_sig,
                       &stopped, &stop_sig);

        if (signaled) {
                pr_info("[program2] : get %s signal\n", signal_to_name(term_sig));
                pr_info("[program2] : child process terminated\n");
                pr_info("[program2] : The return signal is %d\n", term_sig);
        } else if (stopped) {
                pr_info("[program2] : child process stopped by %s\n",
                        signal_to_name(stop_sig));
        } else if (exited) {
                pr_info("[program2] : child process exited normally\n");
                pr_info("[program2] : The return code is %d\n", exit_code);
        } else {
                pr_info("[program2] : unknown child state (status=0x%x)\n", status);
        }

        return 0;
}

static int __init program2_init(void){
        int err;

        pr_info("[program2] : module_init\n");
        worker = kthread_run(run_user_process, NULL, "program2_worker");
        pr_info("[program2] : module_init create kthread start\n");

        if (IS_ERR(worker)) {
                err = PTR_ERR(worker);
                pr_err("[program2] : kthread_run failed: %d\n", err);
                return err;
        }

        return 0;
}

static void __exit program2_exit(void){
        pid_t pid = atomic_read(&child_pid);

        if (pid > 0) {
                struct pid *task_pid = find_vpid(pid);

                if (task_pid) {
                        pr_info("[program2] : module_exit kill child pid=%d\n", pid);
                        kill_pid(task_pid, SIGKILL, 1);
                }
                atomic_set(&child_pid, -1);
        }

        if (worker && !IS_ERR(worker))
                kthread_stop(worker);

        pr_info("[program2] : module_exit./my\n");
}

module_init(program2_init);
module_exit(program2_exit);
