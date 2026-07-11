#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int create_file(const char *path) {
	int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0600);
	if(fd < 0)
		return -1;
	if(write(fd, "x", 1) != 1) {
		int e = errno;
		close(fd);
		errno = e;
		return -1;
	}
	return close(fd);
}

int main(void) {
	char source[96];
	char destination[96];
	char existing[96];
	char replacement[96];
	int pid = getpid();

	snprintf(source, sizeof(source), "/tmp/orlix-renameat2-%d-source", pid);
	snprintf(destination, sizeof(destination), "/tmp/orlix-renameat2-%d-destination", pid);
	snprintf(existing, sizeof(existing), "/tmp/orlix-renameat2-%d-existing", pid);
	snprintf(replacement, sizeof(replacement), "/tmp/orlix-renameat2-%d-replacement", pid);
	unlink(source);
	unlink(destination);
	unlink(existing);
	unlink(replacement);

	if(create_file(source) || renameat2(AT_FDCWD, source, AT_FDCWD, destination, 0))
		return 1;
	if(access(source, F_OK) != -1 || errno != ENOENT || access(destination, F_OK))
		return 2;

	if(create_file(existing) || create_file(replacement))
		return 3;
	errno = 0;
	if(renameat2(AT_FDCWD, replacement, AT_FDCWD, existing, RENAME_NOREPLACE) != -1)
		return 4;
	if(errno != EEXIST)
		return 5;
	if(access(existing, F_OK) || access(replacement, F_OK))
		return 6;

	unlink(destination);
	unlink(existing);
	unlink(replacement);
	return 0;
}
