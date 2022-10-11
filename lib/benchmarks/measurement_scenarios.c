#include "uk/measurement_scenarios.h"
#include "uk/fuse.h"
#include "fcntl.h"
#include "uk/fusedev.h"
#include "uk/helper_functions.h"
#include "uk/time_functions.h"

#include <dirent.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* ukfuse */
#include "uk/fusedev_core.h"
#include "uk/fuse.h"

/*
    Measure removing `amount` files.

    Necessary files are created and deleted by the function.
*/
__nanosec create_files(struct uk_fuse_dev *fusedev, FILES amount) {
	int rc = 0;
	fuse_file_context *fc;
	fuse_file_context dc = { .is_dir = true, .name = "create_files",
		.mode = 0777, .parent_nodeid = 1 };
	__nanosec start = 0, end = 0;
	char *file_name;


	fc = calloc(amount, sizeof(fuse_file_context));
	if(!fc) {
		uk_pr_err("calloc has failed \n");
		return 0;
	}
	rc = uk_fuse_mkdir_request(fusedev, dc.parent_nodeid,
		dc.name, 0777, &dc.nodeid,
		&dc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_mkdir_request has failed \n");
		return 0;
	}

	// initializing file names

	int max_filename_length = 7 + DIGITS(amount - 1);
	// 2D Array
	char *file_names = (char*) malloc(amount*max_filename_length);
	if (!file_names) {
		uk_pr_err("malloc has failed \n");
		goto free_fc;
	}
	init_filenames(amount, max_filename_length, file_names);

	// measuring the creation of `amount` files

	start = _clock();

	for (FILES i = 0; i < amount; i++) {
		file_name = file_names + i * max_filename_length;
		rc = uk_fuse_create_request(fusedev, dc.nodeid,
			file_name, O_WRONLY | O_CREAT | O_EXCL,
			S_IFREG | S_IRWXU | S_IRWXG | S_IRWXO,
			&fc[i].nodeid, &fc[i].fh,
			&fc[i].nlookup);
		if (rc) {
			uk_pr_err("uk_fuse_create_request has failed \n");
			return 0;
		}
		rc = uk_fuse_release_request(fusedev, false,
			fc[i].nodeid, fc[i].fh);
	}

	end = _clock();

	// cleaning up: deleting all files

	for (FILES i = 0; i < amount; i++) {
		char *file_name = file_names + i * max_filename_length;
		rc = uk_fuse_unlink_request(fusedev, file_name,
			false, fc[i].nodeid,
			fc[i].nlookup, dc.nodeid);
		if (rc) {
			uk_pr_err("uk_fuse_mkdir_request has failed \n");
			start = end = 0;
			goto free_fn;
		}
	}
	rc = uk_fuse_unlink_request(fusedev, dc.name, true,
		dc.nodeid, dc.nlookup, dc.parent_nodeid);
	if (rc) {
		uk_pr_err("uk_fuse_unlink_request has failed \n");
		return 0;
	}

free_fn:
	free(file_names);
free_fc:
	free(fc);
	return end - start;
}

// /*
//     Measure creating `amount` files.

//     Necessary files are created and deleted by the function.
// */
// __nanosec remove_files(FILES amount) {
// 	char dir_name[] = "remove_files";
// 	mkdir(dir_name, 0777);
// 	chdir(dir_name);

// 	// initializing file names

//     int max_file_name_length = 7 + DIGITS(amount - 1);
// 	char *file_names = (char*) malloc(amount*max_file_name_length); // 2D array
// 	init_filenames(amount, max_file_name_length, file_names);

//     // creating `amount` empty files

// 	for (FILES i = 0; i < amount; i++) {
// 		FILE *file = fopen(file_names + i*max_file_name_length, "w");
// 	    if (file == NULL) {
//             fprintf(stderr, "Error creating file number %lu.\n", i);
//             exit(EXIT_FAILURE);
//         }
// 		fclose(file);
// 	}

// 	// flush FS buffers and free pagecaches
// 	#ifdef __linux__
// 	system("sync; echo 3 > /proc/sys/vm/drop_caches");
// 	#endif

//     // measuring the delition of `amount` files

//     __nanosec start, end;
// 	start = _clock();
// 	for (FILES i = 0; i < amount; i++) {
// 		char *file_name = file_names + i * max_file_name_length;
// 		if (remove(file_name) != 0) {
// 			fprintf(stderr, "Failed to remove \"%s\" file\n", file_name);
// 		}
// 		#ifdef __linux__
// 		sync();
// 		#endif
// 	}
// 	end = _clock();

//     chdir("..");
//     int ret = rmdir(dir_name);
//     if (ret == -1) {
//         printf("Failed to remove directory %s\n", dir_name);
//     }

// 	free(file_names);

//     return end - start;
// }

// /*
//     Measure listing (e.g. ls command) of files. 'file_amount'
// 	specifies how many files are in the directory.

//     Necessary files are to be created and deleted by the caller
// */
// __nanosec list_dir(FILES file_amount) {
// 	__nanosec start, end;

// 	start = _clock();

// 	DIR *dp;
// 	struct dirent *ep;
// 	dp = opendir ("./");
//     if (dp == NULL) {
// 		fprintf(stderr, "Couldn't open the directory\n");
//     }

// 	char str[256];
// 	#ifdef DEBUGMODE
// 	FILES file_count = 0;
// 	#endif
//     while ( (ep = readdir (dp)) ) {
// 		strcpy(str, ep->d_name); // using filenames somehow
// 		#ifdef DEBUGMODE
// 		file_count++;
// 		#endif
//     }
//     (void) closedir (dp);

// 	end = _clock();

// 	#ifdef DEBUGMODE
// 	assert(file_amount + 2 == file_count);
// 	#endif

//     return end - start;
// }

// /*
//     Measuring sequential write with buffer on heap, allocated with malloc.
// 	Buffer size can be set through 'buffer_size'.

//     Write file is created and deleted by the function.
// */
// __nanosec write_seq(BYTES bytes, BYTES buffer_size) {
// 	FILE *file;

// 	char *buffer = malloc(buffer_size);
// 	if (buffer == NULL) {
// 		fprintf(stderr, "Error! Memory not allocated.\n");
// 		exit(0);
// 	}
// 	BYTES iterations = bytes / buffer_size;
// 	BYTES rest = bytes % buffer_size;

// 	file = fopen("write_data", "w");
// 	#ifdef __linux__
// 	int fd = fileno(file);
// 	#endif
// 	if (file == NULL) {
// 		fprintf(stderr, "Error opening file 'write_data'.\n");
// 		exit(EXIT_FAILURE);
// 	}

// 	__nanosec start, end;
// 	start = _clock();

// 	for (BYTES i = 0; i < iterations; i++) {
// 		fwrite(buffer, buffer_size, 1, file);
// 		#ifdef __linux__
// 		fsync(fd);
// 		#endif
// 	}
// 	if (rest > 0) {
// 		if (rest != fwrite(buffer, sizeof(char), rest, file)) {
// 			fprintf(stderr, "Failed to write the rest of the file\n");
// 		}
// 		#ifdef __linux__
// 		fsync(fd);
// 		#endif
// 	}

// 	end = _clock();

// 	fclose(file);
// 	free(buffer);

// 	if (!remove("write_data") == 0) {
// 		fprintf(stderr, "Failed to remove \"write_data\" file\n");
// 	}

//     return end - start;
// }

// /*
//     Measuring random access write (non-sequential) of an existing file, passed to the function.
//     Seed has to be set by the caller.


//     The function:
//     1. Randomly determines a file position from range [0, EOF - upper_write_limit).
//     2. Writes a random amount of bytes, sampled from range [lower_write_limit, upper_write_limit].
//     3. Repeats steps 1-2 until the 'remaining_bytes' amount of bytes is written.
// */
// __nanosec write_randomly(FILE *file, BYTES remaining_bytes, BYTES buffer_size, BYTES lower_write_limit, BYTES upper_write_limit) {
// 	#ifdef __linux__
// 	int fd = fileno(file);
// 	#endif
// 	BYTES size = get_file_size(file);

// 	__nanosec start, end;

// 	start = _clock();

// 	long int position;
// 	while (remaining_bytes > upper_write_limit) {
// 		position = (long int) sample_in_range(0ULL, size - upper_write_limit);
// 		fseek(file, position, SEEK_SET);
// 		BYTES bytes_to_write = sample_in_range(lower_write_limit, upper_write_limit);
// 		write_bytes(file, bytes_to_write, buffer_size);
// 		#ifdef __linux__
// 		fsync(fd);
// 		#endif
// 		remaining_bytes -= bytes_to_write;
// 	}
// 	position = (long int) sample_in_range(0, size - upper_write_limit);
// 	fseek(file, position, SEEK_SET);
// 	write_bytes(file, remaining_bytes, buffer_size);
// 	#ifdef __linux__
// 	fsync(fd);
// 	#endif

// 	end = _clock();

//     return end - start;
// }

// /*
//     Measure sequential read of `bytes` bytes.
// */
// __nanosec read_seq(FILE *file, BYTES bytes, BYTES buffer_size) {
// 	BYTES iterations = bytes / buffer_size;
//     BYTES rest = bytes % buffer_size;
// 	char *buffer = malloc(buffer_size);
// 	if (buffer == NULL) {
// 		fprintf(stderr, "Error! Memory not allocated. At %s, line %d. \n", __FILE__, __LINE__);
// 		exit(EXIT_FAILURE);
// 	}

//     // measuring sequential read

// 	__nanosec start, end;

// 	start = _clock();

// 	for (BYTES i = 0; i < iterations; i++) {
// 		if (buffer_size != fread(buffer, sizeof(char), buffer_size, file)) {
// 			fprintf(stderr, "Failed to read on iteration #%llu\n", i);
// 		}
// 		// system("sync; echo 1 > /proc/sys/vm/drop_caches");
// 	}
// 	if (rest > 0) {
// 		if (rest != fread(buffer, sizeof(char), rest, file)) {
// 			fprintf(stderr, "Failed to read the rest of the file\n");
// 		}
// 	}

// 	end = _clock();

// 	free(buffer);
//     return end - start;
// }


// /*
//     Measuring random access read (non-sequential). Seed has to be set by the caller.

//     The function:
//     1. Randomly determines a file position from range [0, EOF - upper_read_limit).
//     2. Reads a random amount of bytes, sampled from range [lower_read_limit, upper_read_limit].
//     3. Repeats steps 1-2 until the 'remeaning_bytes' amount of bytes is read.

//     File is provided by the caller.
// */
// __nanosec read_randomly(FILE *file, BYTES remaining_bytes, BYTES buffer_size, BYTES lower_read_limit, BYTES upper_read_limit) {
// 	BYTES size = get_file_size(file);

// 	__nanosec start, end;

// 	start = _clock();

// 	long int position;
// 	while (remaining_bytes > upper_read_limit) {
// 		position = (long int) sample_in_range(0, size - upper_read_limit);
// 		fseek(file, position, SEEK_SET);
// 		BYTES bytes_to_read = sample_in_range(lower_read_limit, upper_read_limit);
// 		// system("sync; echo 1 > /proc/sys/vm/drop_caches");
// 		read_bytes(file, bytes_to_read, buffer_size);
// 		remaining_bytes -= bytes_to_read;
// 	}
// 	position = sample_in_range(0, size - upper_read_limit);
// 	fseek(file, position, SEEK_SET);
// 	read_bytes(file, remaining_bytes, buffer_size);

// 	end = _clock();

//     return end - start;
// }