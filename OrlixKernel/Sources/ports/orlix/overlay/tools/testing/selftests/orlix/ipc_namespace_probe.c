// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <sched.h>
#include <stdbool.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define ORLIX_IPC_SHM_KEY ((key_t)0x4f53484d)
#define ORLIX_IPC_MSG_KEY ((key_t)0x4f4d5347)
#define ORLIX_IPC_MQ_NAME "/orlix-ipc-namespace-probe"

static void remove_shared_memory_key(key_t key)
{
	int id = shmget(key, 1, 0600);

	if (id >= 0)
		(void)shmctl(id, IPC_RMID, NULL);
}

static void remove_message_queue_key(key_t key)
{
	int id = msgget(key, 0600);

	if (id >= 0)
		(void)msgctl(id, IPC_RMID, NULL);
}

static void remove_posix_message_queue_name(const char *name)
{
	(void)mq_unlink(name);
}

static bool ipc_namespace_isolates_shared_memory_key(void)
{
	int parent_id;
	pid_t child;
	int status;

	remove_shared_memory_key(ORLIX_IPC_SHM_KEY);
	parent_id = shmget(ORLIX_IPC_SHM_KEY, 4096, IPC_CREAT | IPC_EXCL | 0600);
	if (parent_id < 0)
		return false;

	child = fork();
	if (child < 0) {
		(void)shmctl(parent_id, IPC_RMID, NULL);
		return false;
	}
	if (child == 0) {
		int child_id;

		if (unshare(CLONE_NEWIPC) != 0)
			_exit(1);
		errno = 0;
		if (shmget(ORLIX_IPC_SHM_KEY, 4096, 0600) >= 0 || errno != ENOENT)
			_exit(2);
		child_id = shmget(ORLIX_IPC_SHM_KEY, 4096,
				  IPC_CREAT | IPC_EXCL | 0600);
		if (child_id < 0)
			_exit(3);
		(void)shmctl(child_id, IPC_RMID, NULL);
		_exit(0);
	}

	if (waitpid(child, &status, 0) != child) {
		(void)shmctl(parent_id, IPC_RMID, NULL);
		return false;
	}
	(void)shmctl(parent_id, IPC_RMID, NULL);
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool ipc_namespace_isolates_message_queue_key(void)
{
	int parent_id;
	pid_t child;
	int status;

	remove_message_queue_key(ORLIX_IPC_MSG_KEY);
	parent_id = msgget(ORLIX_IPC_MSG_KEY, IPC_CREAT | IPC_EXCL | 0600);
	if (parent_id < 0)
		return false;

	child = fork();
	if (child < 0) {
		(void)msgctl(parent_id, IPC_RMID, NULL);
		return false;
	}
	if (child == 0) {
		int child_id;

		if (unshare(CLONE_NEWIPC) != 0)
			_exit(1);
		errno = 0;
		if (msgget(ORLIX_IPC_MSG_KEY, 0600) >= 0 || errno != ENOENT)
			_exit(2);
		child_id = msgget(ORLIX_IPC_MSG_KEY,
				  IPC_CREAT | IPC_EXCL | 0600);
		if (child_id < 0)
			_exit(3);
		(void)msgctl(child_id, IPC_RMID, NULL);
		_exit(0);
	}

	if (waitpid(child, &status, 0) != child) {
		(void)msgctl(parent_id, IPC_RMID, NULL);
		return false;
	}
	(void)msgctl(parent_id, IPC_RMID, NULL);
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool ipc_namespace_isolates_posix_message_queue_name(void)
{
	struct mq_attr attr = {
		.mq_flags = 0,
		.mq_maxmsg = 4,
		.mq_msgsize = 16,
		.mq_curmsgs = 0,
	};
	mqd_t parent_queue;
	pid_t child;
	int status;

	remove_posix_message_queue_name(ORLIX_IPC_MQ_NAME);
	parent_queue = mq_open(ORLIX_IPC_MQ_NAME,
			       O_CREAT | O_EXCL | O_RDWR, 0600, &attr);
	if (parent_queue == (mqd_t)-1)
		return false;

	child = fork();
	if (child < 0) {
		(void)mq_close(parent_queue);
		remove_posix_message_queue_name(ORLIX_IPC_MQ_NAME);
		return false;
	}
	if (child == 0) {
		mqd_t child_queue;

		if (unshare(CLONE_NEWIPC) != 0)
			_exit(1);
		errno = 0;
		if (mq_open(ORLIX_IPC_MQ_NAME, O_RDONLY) != (mqd_t)-1 ||
		    errno != ENOENT)
			_exit(2);
		child_queue = mq_open(ORLIX_IPC_MQ_NAME,
				      O_CREAT | O_EXCL | O_RDWR, 0600, &attr);
		if (child_queue == (mqd_t)-1)
			_exit(3);
		(void)mq_close(child_queue);
		remove_posix_message_queue_name(ORLIX_IPC_MQ_NAME);
		_exit(0);
	}

	if (waitpid(child, &status, 0) != child) {
		(void)mq_close(parent_queue);
		remove_posix_message_queue_name(ORLIX_IPC_MQ_NAME);
		return false;
	}
	(void)mq_close(parent_queue);
	remove_posix_message_queue_name(ORLIX_IPC_MQ_NAME);
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(void)
{
	orlix_test_plan(3);
	orlix_test_result(ipc_namespace_isolates_shared_memory_key(),
			  "IPC namespace isolates SysV shared memory keys");
	orlix_test_result(ipc_namespace_isolates_message_queue_key(),
			  "IPC namespace isolates SysV message queue keys");
	orlix_test_result(ipc_namespace_isolates_posix_message_queue_name(),
			  "IPC namespace isolates POSIX message queue names");
	orlix_test_exit();
}
