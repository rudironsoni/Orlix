// SPDX-License-Identifier: MIT

#include <elf.h>
#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <unistd.h>

static int verify_auxv(int argc, char **argv, char **envp) {
	static const struct option long_options[] = {
		{ "untagged", no_argument, NULL, 'u' },
		{ NULL, 0, NULL, 0 },
	};
	unsigned long page_size = 0;
	unsigned long value = 0;
	int option_count = 0;
	int option;

	if (argc != 8 || strcmp(argv[1], "auxv-reexec") ||
	    strcmp(argv[7], "final-argument")) {
		printf("# auxv argc/argv mismatch argc=%d\n", argc);
		return EXIT_FAILURE;
	}
	if (!envp[0] || strcmp(envp[0], "ORLIX_AUXV_TEST=present") ||
	    !envp[1] || strcmp(envp[1], "SECOND=value") || envp[2]) {
		printf("# auxv environment mismatch\n");
		return EXIT_FAILURE;
	}
	if (!setlocale(LC_ALL, "")) {
		printf("# auxv setlocale failed\n");
		return EXIT_FAILURE;
	}
	while ((option = getopt_long(argc, argv, "a:l:", long_options, NULL)) != -1) {
		if (option != 'u' && option != 'a' && option != 'l') {
			printf("# auxv getopt_long unexpected option=%d\n", option);
			return EXIT_FAILURE;
		}
		option_count++;
	}
	if (option_count != 3) {
		printf("# auxv getopt_long count=%d expected=3\n", option_count);
		return EXIT_FAILURE;
	}

	/* Auxiliary-vector lookup must not reparse application-mutable argv. */
	argv[argc] = (char *)"modified-terminator";

	if (peekauxval(AT_PAGESZ, &page_size) || !page_size ||
	    (page_size & (page_size - 1))) {
		printf("# auxv AT_PAGESZ invalid value=%lu errno=%d\n",
		       page_size, errno);
		return EXIT_FAILURE;
	}
	if (getauxval(AT_PAGESZ) != page_size) {
		printf("# auxv getauxval AT_PAGESZ mismatch\n");
		return EXIT_FAILURE;
	}

	errno = 0;
	if (peekauxval(UINTPTR_MAX, &value) != -1 || errno != ENOENT) {
		printf("# auxv missing entry result mismatch errno=%d\n", errno);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int main(int argc, char **argv, char **envp) {
	if (argc > 1)
		return verify_auxv(argc, argv, envp);

	char *const child_argv[] = {
		argv[0],
		"auxv-reexec",
		"--untagged",
		"-a",
		"bsd",
		"-l0",
		"input",
		"final-argument",
		NULL,
	};
	char *const child_envp[] = {
		"ORLIX_AUXV_TEST=present",
		"SECOND=value",
		NULL,
	};

	execve(argv[0], child_argv, child_envp);
	printf("# auxv self-exec failed errno=%d\n", errno);
	return EXIT_FAILURE;
}
