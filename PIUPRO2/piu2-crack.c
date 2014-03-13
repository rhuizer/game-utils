/* maint. mode: 9, m, M, service, test */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <sys/ptrace.h>
#include <sys/user.h>

//#define LOADER_PATH	"/boot/loader"
#define LOADER_PATH	"/dropbox/loader"

#define DEFINE_XPTRACE_GET_REG(r)					\
long xptrace_get_##r(pid_t pid)						\
{									\
	return xptrace(							\
		PTRACE_PEEKUSER,					\
		pid,							\
		(void *)offsetof(struct user_regs_struct, r),		\
		NULL							\
	);								\
}

long xptrace(enum __ptrace_request request, pid_t pid, void *addr, void *data)
{
	long ret;

	errno = 0;
	ret = ptrace(request, pid, addr, data);
	if (errno != 0 && ret == -1) {
		perror("ptrace()");
		exit(EXIT_FAILURE);
	}

	return ret;
}

DEFINE_XPTRACE_GET_REG(orig_eax);

pid_t xwaitpid(pid_t pid, int *status, int options)
{
	pid_t ret;

	if ( (ret = waitpid(pid, status, options)) == -1) {
		perror("waitpid()");
		exit(EXIT_FAILURE);
	}

	return ret;
}

pid_t spawn_loader(void)
{
	pid_t pid;

	switch (pid = fork()) {
	case -1:
		perror("fork()");
		exit(EXIT_FAILURE);
	case 0:
	        xptrace(PTRACE_TRACEME, 0, NULL, NULL);
		raise(SIGTRAP);

		execl(LOADER_PATH, "loader", NULL);
		exit(EXIT_FAILURE);
	}

	return pid;
}

static inline int __is_execve(int status)
{
	return WIFSTOPPED(status) &&
	       status >> 8 == SIGTRAP | (PTRACE_EVENT_EXEC << 8);
}

static inline int __is_sigtrap(int status)
{
	return WIFSTOPPED(status) && (status >> 8 == SIGTRAP);
}

static inline int __is_sigill(int status)
{
	return WIFSTOPPED(status) && (status >> 8 == SIGILL);
}

static inline int __exited(int status)
{
	return WIFEXITED(status) || WIFSIGNALED(status);
}

static void print_status(int status)
{
	if (WIFEXITED(status)) {
		fprintf(stderr, "Exited with status %d.\n", WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		fprintf(stderr, "Terminated by signal %d.\n", WTERMSIG(status));
	} else if (WIFSTOPPED(status)) {
		fprintf(stderr, "Stopped by signal %d.\n", WSTOPSIG(status));
	} else if (WIFCONTINUED(status)) {
		fprintf(stderr, "Continued.\n");
	} else {
		abort();
	}
}

void ptrace_wait_for_execve(pid_t pid)
{
	int status;

	/* Wait until the execve(). */
	do {
		xwaitpid(pid, &status, 0);
		if (__exited(status)) {
			fprintf(stderr, "[-] Failed to execute loader.\n");
			exit(EXIT_FAILURE);
		}
	} while (!__is_execve(status));

	/* Continue and stop on next syscall. */
	xptrace(PTRACE_SYSCALL, pid, NULL, NULL);
	xwaitpid(pid, &status, 0);

	/* We should trigger the post SYSCALL event for execve(). */
	if (!__is_sigtrap(status)) {
		fprintf(stderr, "[-] Expected a SIGTRAP (status: %d)\n", status);
		exit(EXIT_FAILURE);
	}
}

void ptrace_wait_for_syscall(pid_t pid)
{
	int status;

	xptrace(PTRACE_SYSCALL, pid, NULL, NULL);
	xwaitpid(pid, &status, 0);

	while (__is_sigill(status)) {
		xptrace(PTRACE_SYSCALL, pid, NULL, (void *)SIGILL);
		xwaitpid(pid, &status, 0);
	}

	if (!__is_sigtrap(status)) {
		fprintf(stderr, "[-] Expected a SIGTRAP\n", status);
		print_status(status);
		exit(EXIT_FAILURE);
	}
}

int main(void)
{
	struct user_regs_struct regs;
	int status;
	pid_t pid;

	/* Spawn the loader process. */
	pid = spawn_loader();

        /* Wait until the child is attached with ptrace. */
	xwaitpid(pid, NULL, 0);

        /* Make sure we can detect execve() of the loader. */
        xptrace(PTRACE_SETOPTIONS, pid, NULL, (void *)PTRACE_O_TRACEEXEC);
	xptrace(PTRACE_CONT, pid, NULL, NULL);

	/* Wait for the loader to execute. */
	ptrace_wait_for_execve(pid);

	do {
		ptrace_wait_for_syscall(pid);
	} while (xptrace_get_orig_eax(pid) != __NR_init_module);
}
