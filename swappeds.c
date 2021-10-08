/*
MIT License

Copyright (c) 2021 SSYSS000

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>

#define PROC_PATH	"/proc"
#define FIELD_PNAME	"Name"
#define FIELD_VMSWAP	"VmSwap"

#define eprintf(...) do {						\
		fprintf(stderr, "%s: ", args.command);			\
		fprintf(stderr, __VA_ARGS__);				\
	} while (0)

#define DIE(code, ...) do {						\
		eprintf(__VA_ARGS__);					\
		exit(code);						\
	} while (0)

#define PERROR(str) \
	eprintf("%s: %s\n", str, strerror(errno)) 

struct {
	char *command;
	unsigned produce_total : 1;
	unsigned append_name : 1;
} args;

struct proc_stat {
	int pid;
	char pname[48];
	ssize_t vm_swap;
};

size_t total_swap;

static void print_usage(FILE *file)
{
	fprintf(file,
		"usage:\n %s [options]\n"
		"options:\n"
		" -c\tproduce total\n"
		" -h\tshow this text\n"
		" -n\tshow process name\n",
		args.command);
}

static int is_number(const char *string)
{
	if (*string == '\0')
		return 0;
	
	for (; *string != '\0'; ++string) {
		if (!isdigit(*string))
			return 0;
	}

	return 1;
}

static char *filepath_of(const char *filename, int pid)
{
	static char path[256];
	int rc;

	rc = snprintf(path, sizeof path, "%s/%d/%s", PROC_PATH, pid, filename);
	if (rc < 0)
		DIE(EXIT_FAILURE, "output error\n");
	if (rc >= sizeof path)
		DIE(EXIT_FAILURE, "filepath too long\n");

	return path;
}

static void make_proc_stat(struct proc_stat *pstat)
{
	char buf[512], *field_val;
	FILE *pstatus;

	pstatus = fopen(filepath_of("status", pstat->pid), "r");
	if (!pstatus) { 
		PERROR("cannot read process status");
		exit(EXIT_FAILURE);
	}

	*pstat->pname = '\0';
	pstat->vm_swap = -1;
	while (*pstat->pname == '\0' || pstat->vm_swap == -1) {
		if (fgets(buf, sizeof buf, pstatus) == NULL)
			break;

		if (!strncmp(buf, FIELD_PNAME, sizeof(FIELD_PNAME)-1)) {
			field_val = buf + sizeof(FIELD_PNAME);
			field_val += strspn(field_val, " \t");
			strncpy(pstat->pname, field_val, sizeof(pstat->pname));
			pstat->pname[strcspn(pstat->pname, "\n")] = '\0';
		}
		else if (!strncmp(buf, FIELD_VMSWAP, sizeof(FIELD_VMSWAP)-1)) {
			field_val = buf + sizeof(FIELD_VMSWAP);
			sscanf(field_val, "%zu", &pstat->vm_swap);
		}
	}

	if (ferror(pstatus)) {
		fclose(pstatus);
		DIE(EXIT_FAILURE, "file error\n");
	}

	fclose(pstatus);
}

static void handle_pid(int pid)
{
	struct proc_stat pstat;
	pstat.pid = pid;
	make_proc_stat(&pstat);

	if (pstat.vm_swap > 0) {
		total_swap += pstat.vm_swap;
		printf("%7d\t%7zu kB", pid, pstat.vm_swap);

		if (args.append_name)
			printf("\t%s", pstat.pname);
		putchar('\n');
	}
}

static void scan_args(int argc, char *argv[])
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

	if ((dir = opendir(PROC_PATH)) == NULL) {
		PERROR("cannot open " PROC_PATH);
		return EXIT_FAILURE;
	}

	printf("    PID       SWAP");
	if (args.append_name)
		printf("\tNAME");
	putchar('\n');

	while (1) {
		errno = 0;
		dirent = readdir(dir); 
		if (dirent == NULL)
			break;
		if (is_number(dirent->d_name))
			handle_pid(atoi(dirent->d_name));
	}

	if (!dirent && errno != 0) {
		PERROR("cannot read " PROC_PATH);
		return EXIT_FAILURE;
	}

	closedir(dir);

	if (args.produce_total)
		printf("Total   %7zu kB\n", total_swap);
	
	return EXIT_SUCCESS;
}
