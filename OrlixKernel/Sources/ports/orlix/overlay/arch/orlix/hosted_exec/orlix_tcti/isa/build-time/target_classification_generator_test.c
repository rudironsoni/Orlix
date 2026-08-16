/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_classification_generator.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif

static char *read_file(const char *path, size_t *length)
{
	FILE *file = fopen(path, "rb");
	long size;
	char *data;

	if (!file || fseek(file, 0, SEEK_END) || (size = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET))
		return NULL;
	data = malloc((size_t)size + 1U);
	if (!data || fread(data, 1, (size_t)size, file) != (size_t)size || fclose(file)) {
		free(data);
		return NULL;
	}
	data[size] = '\0';
	*length = (size_t)size;
	return data;
}

int main(int argc, char **argv)
{
	struct orlix_tcti_target_classification_output generated;
	char *current;
	char *input;
	size_t current_length;
	size_t input_length;
	int root_fd;

	if (argc != 4)
		return 2;
	root_fd = open(argv[1], O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (root_fd < 0 || orlix_tcti_target_classification_generate(root_fd, &generated))
		return 1;
	current = read_file(argv[2], &current_length);
	input = read_file(argv[3], &input_length);
	if (!current || !input || input_length != generated.length ||
	    memcmp(input, generated.data, generated.length) ||
	    current_length != generated.length ||
	    memcmp(current, generated.data, generated.length) ||
	    orlix_tcti_target_classification_matches(root_fd, current, current_length))
		return 1;
	current[0] ^= 1;
	if (!orlix_tcti_target_classification_matches(root_fd, current, current_length))
		return 1;
	free(input);
	free(current);
	orlix_tcti_target_classification_destroy(&generated);
	return close(root_fd) ? 1 : 0;
}
