#ifndef _UTILS_INI_H
#define _UTILS_INI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "map.h"

typedef struct ini {
	struct map_s64 sections;
	struct map_str params[64];
	uint32_t size;
} ini;

// Set max line length. If a line is longer, it will be truncated silently.
#define INI_MAX_LINE_LEN 1024

/**
 * @param arg      user arg
 * @param line     current line number
 * @param section  section
 * @param key      key
 * @param value    value
 * @return         Return '0' on success, any other value will make parser
 *                 stop and return error
 */
typedef int (*ini_on_item)(void *arg, int line, const char *section,
			      const char *key, const char *value);

/**
 * @param arg      user data to be passed to 'on_item' callback
 * @param on_item  callback
 * @param filename filename
 * @return         - '0' on success,
 *                 - '-1' on file IO error.
 *                 - positive line number on parsing error
 *                 - 'on_item' return value if it returns other than '0'
 */
int ini_parse_file(void *arg, ini_on_item on_item, const char *filename);

/**
 * @param arg      user data to be passed to 'on_item' callback.
 * @param on_item  callback
 * @param str      string to parse
 * @return         - '0' on success,
 *                 - positive line number on parsing error
 *                 - "on_item's return value" if it returns other than '0'
 */
int ini_parse_string(void *arg, ini_on_item on_item, const char *str);

void ini_init(ini* i);

char* ini_get(ini* i, const char* section, const char* key);
void ini_put(ini* i, const char* section, const char* key, const char* value);
void ini_del(ini* i, const char* section, const char* key);

int ini_load(ini* i, const char* filename);
int ini_save(ini* i, const char* filename);

#ifdef __cplusplus
}
#endif

#if defined(_UTILS_IMPL) || defined(_UTILS_INI_IMPL)

#include <ctype.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#pragma warning(disable : 4996)
#endif

static int ini_default_parse_fn(ini* i, int line, const char *section, const char* key, const char *value) {
	uint32_t index = map_get_s64(&i->sections, section);
	if (!i->sections.found) {
		index = i->size;
		map_put_s64(&i->sections, strdup(section), index);
		i->size++;
	} map_put_str(&i->params[index], strdup(key), strdup(value));
}

void ini_init(ini* i) {
	map_init_s64(&i->sections, 0, 0);
	for (uint32_t index = 0; index < 64; index++) {
		map_init_str(&i->params[index], 0, 0);
	} i->size = 0;
}

void ini_del(ini* i, const char* section, const char* key) {
	uint32_t index = map_get_s64(&i->sections, section);
	if (!i->sections.found) return;

	map_del_str(&i->params[index], key);
	i->size--;

	if (i->params[index].size == 0) {
		map_term_str(&i->params[index]);
		map_del_s64(&i->sections, section);
	}

	if (i->sections.size == 0) {
		map_term_s64(&i->sections);
	}
}

char* ini_get(ini* i, const char* section, const char* key) {
	uint32_t index = map_get_s64(&i->sections, section);
	if (!i->sections.found) return NULL;
	char* val = map_get_str(&i->params[index], key);
	return val;
}

void ini_put(ini* i, const char* section, const char* key, const char* value) {
	ini_default_parse_fn(i, NULL, section, key, value);
}

int ini_load(ini* i, const char* filename) {
	map_term_s64(&i->sections);
	for (uint32_t index = 0; index < i->size; index++) {
		map_term_sv(&i->params[index]);
	} i->size = 0;
	ini_parse_file(i, ini_default_parse_fn, filename);
	return 1;
}

int ini_save(ini* i, const char* filename) {
	if (filename == NULL) 
        return NULL;

    uint32_t index; char* section; char* key; char* value;
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) return NULL;

	map_foreach(&i->sections, section, index) {
		fprintf(fp, "\n[%s]\n", section);
		map_foreach(&i->params[index], key, value) {
			fprintf(fp, "%s=%s\n", key, value);
		}
	}
        
    // close the file
    fclose(fp);
    return 1;
}

static char *trim_space(char *str)
{
	char *end;

	while (isspace((unsigned char) *str)) {
		str++;
	}

	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char) *end)) {
		end--;
	}

	end[1] = '\0';

	return str;
}

static char *trim_comment(char *str)
{
	char *s = str;

	if (*s == '\0' || *s == ';' || *s == '#') {
		*s = '\0';
		return str;
	}

	while (*s && (s = strchr(s, ' ')) != NULL) {
		s++;
		if (*s == ';' || *s == '#') {
			*s = '\0';
			break;
		}
	}

	return str;
}

static char *trim_bom(char *str)
{
	unsigned char *p = (unsigned char *) str;

	if (strlen(str) >= 3) {
		if (p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) {
			str += 3;
		}
	}

	return str;
}

int ini_parse(void *arg, ini_on_item cb, void *arg1,
		 char *(*next_line)(void *, char *, size_t))
{
	int rc = 0, line = 0;
	char buf[INI_MAX_LINE_LEN];
	char section[256] = {0}, key[256] = {0};
	char *head, *end;

	while ((head = next_line(arg1, buf, sizeof(buf) - 1)) != NULL) {
		if (++line == 1) {
			head = trim_bom(head);
		}

		head = trim_space(trim_comment(head));
		if (*head == '\0') {
			continue;
		}

		if (head > buf && *key) {
			rc = cb(arg, line, section, key, head);
		} else if (*head == '[') {
			if ((end = strchr(head, ']')) == NULL) {
				return line;
			}

			*key = '\0';
			*end = '\0';
			strncpy(section, head + 1, sizeof(section) - 1);
		} else {
			if ((end = strpbrk(head, "=:")) == NULL) {
				return line;
			}

			*end = '\0';
			trim_space(head);
			strncpy(key, head, sizeof(key) - 1);
			rc = cb(arg, line, section, head, trim_space(end + 1));
		}

		if (rc != 0) {
			return line;
		}
	}

	return 0;
}

static char *file_line(void *p, char *buf, size_t size)
{
	return fgets(buf, (int) size, (FILE *) p);
}

static char *string_line(void *p, char *buf, size_t size)
{
	size_t len, diff;
	char *t, *str = (*(char **) p);

	if (str == NULL || *str == '\0') {
		return NULL;
	}

	t = strchr(str, '\n');
	if (t == NULL) {
		t = str + strlen(str);
	}

	diff = (size_t) (t - str);
	len = diff < size ? diff : size;
	memcpy(buf, str, len);
	buf[len] = '\0';

	*(char **) p = (*t == '\0') ? '\0' : t + 1;

	return buf;
}

int ini_parse_file(void *arg, ini_on_item cb, const char *filename)
{
	int rc;
	FILE *file;

	file = fopen(filename, "rb");
	if (!file) {
		return -1;
	}

	rc = ini_parse(arg, cb, file, file_line);
	if (rc == 0) {
		rc = ferror(file) != 0 ? -1 : 0;
	}

	fclose(file);

	return rc;
}

int ini_parse_string(void *arg, ini_on_item cb, const char *str)
{
	char *ptr = (char *) str;
	return ini_parse(arg, cb, &ptr, string_line);
}

#endif
#endif