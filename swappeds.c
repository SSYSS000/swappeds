/*
MIT License

Copyright (c) 2023 SSYSS000

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
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>

#if !defined(__linux__)
#warning "This system is not supported."
#endif /* !defined(__linux__) */

#define eprintf(...)	fprintf(stderr, ##__VA_ARGS__)

struct {
	char *command;
	unsigned produce_total : 1;
} args;

struct process_status {
	int pid;
	char process_name[48];
	size_t vm_swap;
};

static int print_usage(FILE *file)
{
	int n;

	n = fprintf(file,
			"usage:\n %s [options]\n"
			"options:\n"
			" -c\tproduce total\n"
			" -h\tshow this text\n",
			args.command);

	return n;
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
		default:
			print_usage(stderr);
			exit(EXIT_FAILURE);
		}
	}
}

static int print_column_titles(void)
{
	int n;
	n = printf("    PID       SWAP\tNAME\n");
	return n;
}

static int print_row(const struct process_status *status)
{
	int n;
	n = printf("%7d\t%7zu kB\t%s\n", status->pid, status->vm_swap, status->process_name);
	return n;
}

static char *find_not_qualifying(const char *string, int (*qualify)(int))
{
	while (qualify((unsigned char)*string))
		string++;

	// Caller's responsibility not to write to read-only memory.
	return (char *)string;
}

#define find_non_digit(str) find_not_qualifying(str, isdigit)

// On systems that support procfs, each process status is in a file under
// the pid subdirectory under /proc. Hence, DIR is used to iterate over
// the process statuses.
typedef DIR process_status_reader_t;

static process_status_reader_t *create_process_status_reader(void)
{
	DIR *proc;

	proc = opendir("/proc");

	if (!proc) {
		perror("opendir /proc");
	}

	return proc;
}

static void destroy_process_status_reader(process_status_reader_t *dir)
{
	closedir(dir);
}

static struct dirent *read_next_pid_subdir(DIR *proc)
{
	struct dirent *dirent;

	// In the loop below, look for a pid subdirectory.
	for (;;) {
		errno  = 0;
		dirent = readdir(proc); 

		if (!dirent) {
			// Directory stream ended or an error occurred.
			break;
		}

		// Type should be directory or unknown if not supported.
		if (!(dirent->d_type == DT_DIR || dirent->d_type == DT_UNKNOWN)) {
			continue;
		}

		// Directory filename contains digits only.
		if (*find_non_digit(dirent->d_name) != '\0') {
			continue;
		}

		// Found a pid subdirectory.
		break;
	}

	if (!dirent && errno != 0) {
		perror("readdir /proc");
	}

	return dirent;
}

static FILE *open_process_status_in_dir(const char *pid_subdir_filename)
{
	char path[256];
	FILE *file;
	int n;

	// Write the file path to the process status in a buffer.
	n = snprintf(path, sizeof path, "/proc/%s/status", pid_subdir_filename);

	if (n < 0) {
		eprintf("output error occurred while writing path to process status.\n");
		return NULL;
	}
	else if (n >= sizeof path) {
		eprintf("file path too long\n");
		return NULL;
	}

	file = fopen(path, "r");

	if (!file) {
		perror(path);
	}

	return file;
}

static int
next_process_status(process_status_reader_t *proc, struct process_status *status)
{
	int file_error, fields_not_found;
	struct dirent *dirent;
	FILE *proc_status_fp;
	char line[256];

	// Obtain the filename of the next PID subdirectory.
	dirent = read_next_pid_subdir(proc);

	if (!dirent) {
		return -1;
	}

	proc_status_fp = open_process_status_in_dir(dirent->d_name);

	if (!proc_status_fp) { 
		return -1;
	}

	// Start reading the contents into the status struct.
	memset(status, 0, sizeof(*status));

	fields_not_found = 3;
	while (fields_not_found > 0) {
		if (fgets(line, sizeof line, proc_status_fp) == NULL) {
			break;
		}

		if (sscanf(line, "Name: %47s", status->process_name) == 1) {
			fields_not_found--;
		}
		else if(sscanf(line, "Pid: %d", &status->pid) == 1) {
			fields_not_found--;
		}
		else if (sscanf(line, "VmSwap: %zu", &status->vm_swap) == 1) {
			fields_not_found--;
		}
	}

	file_error = ferror(proc_status_fp);

	(void) fclose(proc_status_fp);

	if (file_error) {
		eprintf("file error\n");
		return -1;
	}

	if (fields_not_found > 0) {
		// FIXME: not all processes have VmSwap field.
		//eprintf("not all fields were found in status file.\n");
	}

	return 0;
}

int main(int argc, char *argv[])
{
	process_status_reader_t *reader;
	struct process_status status;
	size_t total_swap = 0;

	scan_args(argc, argv);

	reader = create_process_status_reader();

	if (!reader) {
		return EXIT_FAILURE;
	}

	if (print_column_titles() < 0) {
		return EXIT_FAILURE;
	}

	while (next_process_status(reader, &status) == 0) {
		if (status.vm_swap > 0) {
			total_swap += status.vm_swap;

			if (print_row(&status) < 0) {
				return EXIT_FAILURE;
			}
		}
	}

	destroy_process_status_reader(reader);

	if (args.produce_total) {
		if (printf("Total   %7zu kB\n", total_swap) < 0) {
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
