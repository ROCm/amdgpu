dnl #
dnl # commit 6b1c66c9cca99 ("exec_state: relocate dumpable information")
dnl # v7.1-rc4-4-g6b1c66c9cca9
dnl # Task dumpability was moved out of mm_struct into a per-task
dnl # struct task_exec_state, replacing get_dumpable()/SUID_DUMP_USER and
dnl # mm->user_ns with task_exec_state_get_dumpable() and
dnl # task->exec_state->{dumpable,user_ns}.
dnl #
AC_DEFUN([AC_AMDGPU_TASK_EXEC_STATE_GET_DUMPABLE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/sched.h>
			#include <linux/sched/coredump.h>
			#include <linux/sched/exec_state.h>
		], [
			struct task_exec_state *es = NULL;
			enum task_dumpable d;

			d = task_exec_state_get_dumpable((struct task_struct *)NULL);
			d = es->dumpable;
			(void)es->user_ns;
			(void)d;
		], [
			AC_DEFINE(HAVE_TASK_EXEC_STATE_DUMPABLE, 1,
				[struct task_exec_state (per-task dumpability) is available])
		])
	])
])
