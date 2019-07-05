#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define PROGRAM_NAME "reopen"

static const char path_xdg[] = "/" PROGRAM_NAME "/config";
static const char path_home[] = "/.config/" PROGRAM_NAME "/config";

char *config_location() {
	const char *xdg = getenv("XDG_CONFIG_HOME");
	if (xdg) {
		size_t xdg_len = strlen(xdg);
		char *ret = malloc(xdg_len + sizeof(path_xdg));
		strncpy(ret, xdg, xdg_len);
		strncpy(ret + xdg_len, path_xdg, sizeof(path_xdg));
		return ret;
	}

	const char *home = getenv("HOME");
	if (home) {
		size_t home_len = strlen(home);
		char *ret = malloc(home_len + sizeof(path_home));
		strncpy(ret, home, home_len);
		strncpy(ret + home_len, path_home, sizeof(path_home));
		return ret;
	}

	return NULL;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: " PROGRAM_NAME " <URL or path>\n");
		return EXIT_FAILURE;
	}

	char *config_file = config_location();
	if (!config_file) {
		fprintf(stderr,
			"neither HOME nor XDG_CONFIG_HOME set, don't know where to find "
			"config file");
		return EXIT_FAILURE;
	}

	FILE *f = fopen(config_file, "r");
	free(config_file);
	if (!f) {
		perror("failed to open config file");
		return EXIT_FAILURE;
	}

	char *line = NULL;
	ssize_t len = 0;
	size_t size = 0;
	while ((len = getline(&line, &size, f)) != EOF) {
		char *p = line;
		char *regex = NULL;

		if (*p++ != '/') break;
		for (; *p; p++)
			if (*p == '/' && *(p - 1) != '\\') break;
		if (!*p) break;
		if (*p++ != '/') break;
		if (*p++ != ' ') break;

		if (line[len - 1] == '\n') line[len - 1] = '\0';

		regex = strndup(line + 1, p - line - 3);

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
			size_t p_len = strlen(p);
			char *buf = malloc(p_len + 1 + strlen(argv[1]) + 1);

			strcpy(buf, p);
			buf[p_len] = ' ';
			strcpy(buf + p_len + 1, argv[1]);

			free(line);
			fclose(f);

			execl("/bin/sh", "sh", "-c", buf, NULL);

			free(buf);
			perror("execl");
			return EXIT_FAILURE;
		};

		regfree(&re);
	}

	free(line);
	fclose(f);

	fprintf(stderr, "no handler found for that filetype\n");
	return EXIT_FAILURE;
}
