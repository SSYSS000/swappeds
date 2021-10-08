#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>

#define eprintf(...) do {			\
	fprintf(stderr, "%s: ", args.command);	\
	fprintf(stderr, __VA_ARGS__);		\
	} while (0)

#define PERROR(str) \
	eprintf("%s: %s\n", str, strerror(errno)) 

struct {
	char *command;
	unsigned produce_total : 1;
	unsigned append_name : 1;
} args;

size_t total_swap;

void print_usage(FILE *file)
{
	fprintf(file,
	"usage:\n %s [options]\n"
	"options:\n"
	" -c\tproduce total\n"
	" -h\tshow this text\n"
	" -n\tshow process name\n",
	args.command);
}

int is_number(const char *string)
{
	const char *i;
	for (i = string; *i != '\0'; ++i) {
		if (!isdigit(*i))
			return 0;
	}

	return 1;
}

void handle_pid(int pid)
{
	char buf[512], name[48];
	int n_matched;
	FILE *status;
	size_t swap;

	if (snprintf(buf, sizeof buf, "/proc/%d/status", pid) < 0) {
		eprintf("snprintf failed.\n");
		exit(EXIT_FAILURE);
	}
	
	status = fopen(buf, "r");
	if (!status) 
		PERROR(buf);

	name[0] = 0;
	swap = 0;
	while (fgets(buf, sizeof buf, status) != NULL) {
		if (*name == 0 && !strncmp(buf, "Name", 4)) {
			strcpy(name, buf+5+strspn(buf + 5, " \t"));
			continue;
		}

		if (!strncmp(buf, "VmSwap", 6)) {
			sscanf(buf + 7, "%zu", &swap);
			total_swap += swap;
			break;
		}
	}

	if (ferror(status)) {
		eprintf("file error\n");
		exit(EXIT_FAILURE);
	}

	if (swap) {
		if (args.append_name)
			printf("%7d\t%7zu kB\t%s", pid, swap, name);
		else
			printf("%7d\t%7zu kB\n", pid, swap);
	}

	fclose(status);
}

void scan_args(int argc, char *argv[])
{
	int opt;

	args.command = argv[0];

	while ((opt = getopt(argc, argv, "chn")) != -1) {
		switch (opt) {
		case 'c':
			args.produce_total = 1;
			break;
		case 'h':
			print_usage(stdout);
			exit(EXIT_SUCCESS);
			break;
		case 'n':
			args.append_name = 1;
			break;
		default:
			print_usage(stderr);
		 	exit(EXIT_FAILURE);
		}
	}
}

int main(int argc, char *argv[])
{
	struct dirent *dirent;
	DIR *dir;

	scan_args(argc, argv);

	if ((dir = opendir("/proc")) == NULL) {
		PERROR("cannot open /proc");
		exit(EXIT_FAILURE);
	}

	printf("    PID       SWAP");
	if (args.append_name)
		printf("\tNAME");
	putchar('\n');

	while (dirent = readdir(dir)) {
		if (is_number(dirent->d_name))
			handle_pid(atoi(dirent->d_name));
	}
	
	closedir(dir);

	if (args.produce_total)
		printf("Total   %7zu kB\n", total_swap);
	
	return EXIT_SUCCESS;
}
