#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define PROGRAM_NAME "reopen"

static const char path_xdg[] = "/" PROGRAM_NAME "/config";
static const char path_home[] = "/.config/" PROGRAM_NAME "/config";
static const char cmd_append[] = " \"$REOPEN_URI\"";

char *config_location() {
	const char *path;
	if ((path = getenv("XDG_CONFIG_HOME"))) {
		size_t len = strlen(path);
		char *ret = malloc(len + sizeof(path_xdg));
		if (!ret) {
			perror("malloc");
			return NULL;
		}
		memcpy(ret, path, len);
		memcpy(ret + len, path_xdg, sizeof(path_xdg));
		return ret;
	} else if ((path = getenv("HOME"))) {
		size_t len = strlen(path);
		char *ret = malloc(len + sizeof(path_home));
		if (!ret) {
			perror("malloc");
			return NULL;
		}
		memcpy(ret, path, len);
		memcpy(ret + len, path_home, sizeof(path_home));
		return ret;
	} else {
		fprintf(stderr, "neither XDG_CONFIG_HOME or HOME are set");
		return NULL;
	}
}

int main(int argc, char **argv) {
	if (argc < 2) {
		if (argc)
			fprintf(stderr, "usage: %s <URI or path>\n", argv[0]);
		else
			fprintf(stderr, "usage: " PROGRAM_NAME " <URI or path>\n");
		return EXIT_FAILURE;
	}

	char *config_file = config_location();
	if (!config_file) {
		fprintf(stderr, "couldn't find config file\n");
		return EXIT_FAILURE;
	}

	FILE *f = fopen(config_file, "r");
	free(config_file);
	if (!f) {
		perror("couldn't open config file");
		return EXIT_FAILURE;
	}

	char *line = NULL;
	ssize_t len = 0;
	size_t size = 0;

	while ((len = getline(&line, &size, f)) != EOF) {
		char *p = line;

		if (*p++ != '/') continue;

		for (; *p; p++)
			if (*p == '/' && *(p - 1) != '\\') break;

		if (!*p || *p++ != '/') continue;

		char *regex_end = p;

		for (; *p; p++)
			if (*p != ' ' && *p != '\t') break;

		if (!*p) continue;

		if (line[len - 1] == '\n') line[len - 1] = '\0';

		char *regex = strndup(line + 1, regex_end - line - 2);
		regex_t re;
		int ret;

		if ((ret = regcomp(&re, regex, REG_EXTENDED | REG_NOSUB))) {
			char buf[256];

			regerror(ret, &re, buf, sizeof(buf));
			fprintf(stderr, "error compiling regex /%s/: %s\n", regex, buf);

			free(regex);
			free(line);
			fclose(f);

			return EXIT_FAILURE;
		};

		free(regex);

		if (!(ret = regexec(&re, argv[1], 0, NULL, 0))) {
			size_t sh_len = strlen(p);
			char *buf = malloc(sh_len + sizeof(cmd_append) + 1);

			if (!buf) {
				perror("malloc");
				goto cleanup;
			}

			memcpy(buf, p, sh_len);
			memcpy(buf + sh_len, cmd_append, sizeof(cmd_append));

			setenv("REOPEN_URI", argv[1], 1);
			execl("/bin/sh", "sh", "-c", buf, (char *)NULL);

			free(buf);
			perror("execl");

		cleanup:
			regfree(&re);
			free(line);
			fclose(f);
			return EXIT_FAILURE;
		};

		regfree(&re);
	}

	free(line);
	fclose(f);

	fprintf(stderr, "no handler found for that URI or filetype\n");
	return EXIT_FAILURE;
}
