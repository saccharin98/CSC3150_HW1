#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/signal.h>
#include <linux/pid.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/jiffies.h>
#include <linux/kmod.h>
#include <linux/fs.h>
#include <linux/namei.h>

MODULE_LICENSE("GPL");

/*
 * Path of the user-space test program.  The default value matches the
 * reference environment described in the assignment handout but can be
 * overridden at module loading time, e.g.:
 *
 *     sudo insmod program2.ko user_prog=/absolute/path/to/test
 */
static char *user_prog = "/workspace/CSC3150_HW1/program2/test";
module_param(user_prog, charp, 0644);
MODULE_PARM_DESC(user_prog, "Absolute path of the user-space test program");

#define UMH_WTERMSIG(status) ((status) & 0x7f)
#define UMH_WCOREDUMP(status) ((status) & 0x80)
#define UMH_WEXITSTATUS(status) (((status) >> 8) & 0xff)
#define UMH_WSTOPSIG(status) UMH_WEXITSTATUS(status)
#define UMH_WIFEXITED(status) (UMH_WTERMSIG(status) == 0)
#define UMH_WIFSTOPPED(status) (((status) & 0xff) == 0x7f)
#define UMH_WIFCONTINUED(status) ((status) == 0xffff)
#define UMH_WIFSIGNALED(status) \
        (!(UMH_WIFEXITED(status) || UMH_WIFSTOPPED(status) || \
           UMH_WIFCONTINUED(status)))


//implement fork function
int my_fork(void *argc){
        char *path = argc;
        char *argv[2];
        static char *const envp[] = {
                "HOME=/",
                "TERM=linux",
                "PATH=/sbin:/bin:/usr/sbin:/usr/bin",
                NULL,
        };
        int status = 0;

        if (!path || !*path) {
                pr_err("[program2] : invalid user program path\n");
                goto out_free;
        }

        //set default sigaction for current process
        int i;
        struct k_sigaction *k_action = &current->sighand->action[0];
        for(i=0;i<_NSIG;i++){
                k_action->sa.sa_handler = SIG_DFL;
                k_action->sa.sa_flags = 0;
                k_action->sa.sa_restorer = NULL;
                sigemptyset(&k_action->sa.sa_mask);
                k_action++;
        }

        argv[0] = path;
        argv[1] = NULL;

        {
                struct path user_path;
                int err;

                err = kern_path(path, LOOKUP_FOLLOW, &user_path);
                if (err) {
                        pr_err("[program2] : failed to resolve %s (err=%d)\n",
                               path, err);
                        goto out_free;
                }
                path_put(&user_path);
        }

        pr_info("[program2] : launching %s\n", path);

        status = call_usermodehelper(path, argv, (char **)envp, UMH_WAIT_PROC);
        if (status < 0) {
                pr_err("[program2] : failed to execute %s (err=%d)\n", path, status);
                goto out_free;
        }

        if (UMH_WIFEXITED(status)) {
                pr_info("[program2] : child exited with status %d\n",
                        UMH_WEXITSTATUS(status));
        } else if (UMH_WIFSIGNALED(status)) {
                pr_info("[program2] : child terminated by signal %d%s\n",
                        UMH_WTERMSIG(status),
                        UMH_WCOREDUMP(status) ? " (core dumped)" : "");
        } else if (UMH_WIFSTOPPED(status)) {
                pr_info("[program2] : child stopped by signal %d\n",
                        UMH_WSTOPSIG(status));
        } else if (UMH_WIFCONTINUED(status)) {
                pr_info("[program2] : child continued\n");
        } else {
                pr_info("[program2] : child finished with status 0x%x\n", status);
        }

out_free:
        kfree(path);
        return 0;
}

static int __init program2_init(void){

        printk("[program2] : Module_init\n");

        /* create a kernel thread to run my_fork */
        {
                struct task_struct *task;
                char *path_copy;

                path_copy = kstrdup(user_prog, GFP_KERNEL);
                if (!path_copy) {
                        pr_err("[program2] : failed to allocate memory for path\n");
                        return -ENOMEM;
                }

                task = kthread_run(my_fork, path_copy, "program2_fork");
                if (IS_ERR(task)) {
                        pr_err("[program2] : failed to create kernel thread (%ld)\n",
                               PTR_ERR(task));
                        kfree(path_copy);
                        return PTR_ERR(task);
                }
        }

        return 0;
}

static void __exit program2_exit(void){
	printk("[program2] : Module_exit\n");
}

module_init(program2_init);
module_exit(program2_exit);
