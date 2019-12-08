#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>

#include "skynet_daemon.h"

/** 通过 pidfile 检查pid对应的进程是否存在
 *		存在返回 pid, 不存在,返回0
 * 流程:
 * 1. pid文件是否存在,不存在返回0
 * 2. 读取文件中的pid, 如果读不到返回0
 * 3. 通过 kill -0 pid, 检查进程是否存在. 不存在返回0
 * 4. 返回 pid
*/
static int
check_pid(const char *pidfile) {
	int pid = 0;
	FILE *f = fopen(pidfile,"r");
	if (f == NULL)
		return 0;
	int n = fscanf(f,"%d", &pid);
	fclose(f);

	if (n !=1 || pid == 0 || pid == getpid()) {
		return 0;
	}

	/** 检测 pid 对应的进程是否存在
	 * 返回值: 0 成功; -1 失败, 	
	 * RETURN VALUE
	 * 	On  success  (at least one signal was sent), zero is returned.  On error, -1
	 * 	is returned, and errno is set appropriately.
	 * ERRORS
	 * 	EINVAL An invalid signal was specified.
	 * 	EPERM  The process does not have permission to send the signal to any of the
	 * 		target processes.
	 * 	ESRCH  The  pid  or  process  group  does  not exist. Note that an existing
	 * 		process might be a zombie, a process which already committed termina‐tion, but has not yet been wait(2)ed for.
	*/
	if (kill(pid, 0) && errno == ESRCH)
		return 0;

	return pid;
}

/** 将 pid 写入文件 pidfile */
static int
write_pid(const char *pidfile) {
	FILE *f;
	int pid = 0;
	int fd = open(pidfile, O_RDWR|O_CREAT, 0644);
	if (fd == -1) {
		fprintf(stderr, "Can't create pidfile [%s].\n", pidfile);
		return 0;
	}
	f = fdopen(fd, "w+");
	if (f == NULL) {
		fprintf(stderr, "Can't open pidfile [%s].\n", pidfile);
		return 0;
	}

	// 加锁
	if (flock(fd, LOCK_EX|LOCK_NB) == -1) {
		int n = fscanf(f, "%d", &pid);
		fclose(f);
		if (n != 1) {
			fprintf(stderr, "Can't lock and read pidfile.\n");
		} else {
			fprintf(stderr, "Can't lock pidfile, lock is held by pid %d.\n", pid);
		}
		return 0;
	}

	// 写 pid
	pid = getpid();
	if (!fprintf(f,"%d\n", pid)) {
		fprintf(stderr, "Can't write pid.\n");
		close(fd);
		return 0;
	}
	fflush(f);

	return pid;
}

/** 重定向 stdin, stdout, stderr 到 /dev/null, 即忽略
*/
static int
redirect_fds() {
	int nfd = open("/dev/null", O_RDWR);
	if (nfd == -1) {
		perror("Unable to open /dev/null: ");
		return -1;
	}
	if (dup2(nfd, 0) < 0) {
		perror("Unable to dup2 stdin(0): ");
		return -1;
	}
	if (dup2(nfd, 1) < 0) {
		perror("Unable to dup2 stdout(1): ");
		return -1;
	}
	if (dup2(nfd, 2) < 0) {
		perror("Unable to dup2 stderr(2): ");
		return -1;
	}

	close(nfd);

	return 0;
}

int
daemon_init(const char *pidfile) {
	int pid = check_pid(pidfile);

	if (pid) {
		fprintf(stderr, "Skynet is already running, pid = %d.\n", pid);
		return 1;
	}

#ifdef __APPLE__
	fprintf(stderr, "'daemon' is deprecated: first deprecated in OS X 10.5 , use launchd instead.\n");
#else
	if (daemon(1,1)) {
		fprintf(stderr, "Can't daemonize.\n");
		return 1;
	}
#endif

	pid = write_pid(pidfile);
	if (pid == 0) {
		return 1;
	}

	if (redirect_fds()) {
		return 1;
	}

	return 0;
}

int
daemon_exit(const char *pidfile) {
	return unlink(pidfile);
}
