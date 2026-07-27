// SPDX-License-Identifier: MIT

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct strings {
	char **items;
	size_t count;
};

struct macro {
	char *name;
	struct strings permissions;
};

struct macros {
	struct macro *items;
	size_t count;
};

struct class_entry {
	char *name;
	struct strings permissions;
};

struct classes {
	struct class_entry *items;
	size_t count;
};

static void fail(const char *message, const char *detail)
{
	if (detail)
		fprintf(stderr, "%s '%s'\n", message, detail);
	else
		fprintf(stderr, "%s\n", message);
	exit(1);
}

static void *checked_realloc(void *pointer, size_t size)
{
	void *result = realloc(pointer, size);
	if (!result)
		fail("out of memory", NULL);
	return result;
}

static char *copy_range(const char *begin, const char *end)
{
	size_t length = (size_t)(end - begin);
	char *copy = checked_realloc(NULL, length + 1);
	memcpy(copy, begin, length);
	copy[length] = '\0';
	return copy;
}

static void strings_push(struct strings *strings, const char *begin, const char *end)
{
	strings->items = checked_realloc(strings->items,
		(strings->count + 1) * sizeof(*strings->items));
	strings->items[strings->count++] = copy_range(begin, end);
}

static char *read_file(const char *path)
{
	FILE *file = fopen(path, "rb");
	long size;
	char *contents;

	if (!file) {
		fprintf(stderr, "open %s: %s\n", path, strerror(errno));
		exit(1);
	}
	if (fseek(file, 0, SEEK_END))
		fail("failed to measure input", path);
	size = ftell(file);
	if (size < 0 || fseek(file, 0, SEEK_SET))
		fail("failed to measure input", path);
	contents = checked_realloc(NULL, (size_t)size + 1);
	if (fread(contents, 1, (size_t)size, file) != (size_t)size)
		fail("failed to read input", path);
	contents[size] = '\0';
	fclose(file);
	return contents;
}

static void splice_continuations(char *text)
{
	char *source = text;
	char *destination = text;

	while (*source) {
		if (source[0] == '\\' && source[1] == '\n') {
			*destination++ = ' ';
			source += 2;
		} else {
			*destination++ = *source++;
		}
	}
	*destination = '\0';
}

static const struct macro *find_macro(const struct macros *macros,
	const char *begin, const char *end)
{
	for (size_t index = 0; index < macros->count; ++index) {
		const char *name = macros->items[index].name;
		if (strlen(name) == (size_t)(end - begin) && !memcmp(name, begin, (size_t)(end - begin)))
			return &macros->items[index];
	}
	return NULL;
}

static void parse_string_list(const char *begin, const char *end,
	const struct macros *macros, struct strings *result)
{
	while (begin < end) {
		const char *token_end = memchr(begin, ',', (size_t)(end - begin));
		const struct macro *macro;
		if (!token_end)
			token_end = end;
		while (begin < token_end && isspace((unsigned char)*begin))
			++begin;
		while (token_end > begin && isspace((unsigned char)token_end[-1]))
			--token_end;
		if (begin < token_end && !(token_end - begin == 4 && !memcmp(begin, "NULL", 4))) {
			if (token_end - begin >= 2 && begin[0] == '"' && token_end[-1] == '"') {
				strings_push(result, begin + 1, token_end - 1);
			} else if ((macro = find_macro(macros, begin, token_end))) {
				for (size_t index = 0; index < macro->permissions.count; ++index)
					strings_push(result, macro->permissions.items[index],
						macro->permissions.items[index] + strlen(macro->permissions.items[index]));
			} else {
				char *token = copy_range(begin, token_end);
				fail("unknown SELinux permission token", token);
			}
		}
		begin = token_end < end ? token_end + 1 : end;
	}
}

static void parse_macros(char *classmap, struct macros *macros)
{
	for (char *line = classmap; line && *line;) {
		char *next = strchr(line, '\n');
		char *end = next ? next : line + strlen(line);
		const char prefix[] = "#define";
		char *cursor = line;

		if ((size_t)(end - line) >= sizeof(prefix) - 1 && !memcmp(line, prefix, sizeof(prefix) - 1)) {
			cursor += sizeof(prefix) - 1;
			while (cursor < end && isspace((unsigned char)*cursor))
				++cursor;
			char *name_begin = cursor;
			while (cursor < end && (isalnum((unsigned char)*cursor) || *cursor == '_'))
				++cursor;
			if ((size_t)(cursor - name_begin) > strlen("COMMON_") &&
				!memcmp(name_begin, "COMMON_", strlen("COMMON_"))) {
				struct macro *entry;
				macros->items = checked_realloc(macros->items,
					(macros->count + 1) * sizeof(*macros->items));
				entry = &macros->items[macros->count++];
				memset(entry, 0, sizeof(*entry));
				entry->name = copy_range(name_begin, cursor);
				while (cursor < end && isspace((unsigned char)*cursor))
					++cursor;
				parse_string_list(cursor, end, macros, &entry->permissions);
			}
		}
		line = next ? next + 1 : NULL;
	}
}

static void parse_classes(const char *classmap, const struct macros *macros,
	struct classes *classes)
{
	const char *cursor = classmap;

	while ((cursor = strchr(cursor, '{'))) {
		const char *name_begin;
		const char *name_end;
		const char *permissions_begin;
		const char *permissions_end;
		struct class_entry *entry;

		++cursor;
		while (isspace((unsigned char)*cursor))
			++cursor;
		if (*cursor != '"')
			continue;
		++cursor;
		name_begin = cursor;
		name_end = strchr(name_begin, '"');
		if (!name_end)
			break;
		cursor = name_end + 1;
		while (isspace((unsigned char)*cursor))
			++cursor;
		if (*cursor++ != ',')
			continue;
		while (isspace((unsigned char)*cursor))
			++cursor;
		if (*cursor++ != '{')
			continue;
		permissions_begin = cursor;
		permissions_end = strchr(permissions_begin, '}');
		if (!permissions_end)
			break;
		classes->items = checked_realloc(classes->items,
			(classes->count + 1) * sizeof(*classes->items));
		entry = &classes->items[classes->count++];
		memset(entry, 0, sizeof(*entry));
		entry->name = copy_range(name_begin, name_end);
		parse_string_list(permissions_begin, permissions_end, macros, &entry->permissions);
		if (!entry->permissions.count) {
			free(entry->name);
			--classes->count;
		}
		cursor = permissions_end + 1;
	}
}

static void parse_initial_sids(char *text, struct strings *sids)
{
	for (char *line = text; line && *line;) {
		char *next = strchr(line, '\n');
		char *end = next ? next : line + strlen(line);
		char *cursor = line;
		while (cursor < end && isspace((unsigned char)*cursor))
			++cursor;
		if (cursor < end && *cursor++ == '"') {
			char *name_end = memchr(cursor, '"', (size_t)(end - cursor));
			if (name_end) {
				char *after = name_end + 1;
				while (after < end && isspace((unsigned char)*after))
					++after;
				if (after < end && *after == ',')
					strings_push(sids, cursor, name_end);
			}
		}
		line = next ? next + 1 : NULL;
	}
}

int main(int argc, char **argv)
{
	struct macros macros = {0};
	struct classes classes = {0};
	struct strings sids = {0};
	char *classmap;
	char *initial_sids;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <classmap.h> <initial_sid_to_string.h>\n", argv[0]);
		return 2;
	}
	classmap = read_file(argv[1]);
	initial_sids = read_file(argv[2]);
	splice_continuations(classmap);
	parse_macros(classmap, &macros);
	parse_classes(classmap, &macros, &classes);
	parse_initial_sids(initial_sids, &sids);

	puts("# Generated from upstream Linux SELinux class and initial SID maps.");
	puts("# Do not edit this generated prelude; edit the Linux source or policy body.");
	for (size_t index = 0; index < classes.count; ++index)
		printf("class %s\n", classes.items[index].name);
	putchar('\n');
	for (size_t index = 0; index < sids.count; ++index)
		printf("sid %s\n", sids.items[index]);
	putchar('\n');
	for (size_t index = 0; index < classes.count; ++index) {
		printf("class %s {", classes.items[index].name);
		for (size_t permission = 0; permission < classes.items[index].permissions.count; ++permission)
			printf(" %s", classes.items[index].permissions.items[permission]);
		puts(" }");
	}
	printf("\n# ORLIX_SELINUX_ALL_CLASSES");
	for (size_t index = 0; index < classes.count; ++index)
		printf(" %s", classes.items[index].name);
	putchar('\n');
	return 0;
}
