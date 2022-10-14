#include "uk/helper_functions.h"
#include "uk/fuse.h"
#include "uk/fusedev_core.h"
#include "uk/print.h"
#include "uk/vfdev.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// BYTES get_file_size(FILE *file) {
// 	fseek(file, 0L, SEEK_END);
// 	size_t size = ftell(file);
// 	rewind(file);
// 	return size;
// }


// /*
//    Samples natrual numbers in range [lower; upper)
// */
// BYTES sample_in_range(BYTES lower, BYTES upper) {
// 	return (rand() % (upper - lower)) + lower;
// }

// /*
//     Reads `bytes` bytes with a 1KB buffer.
// */
// void read_bytes(FILE *file, BYTES bytes, BYTES buffer_size) {
// 	char *buffer = malloc(buffer_size);
// 	if (buffer == NULL) {
// 		fprintf(stderr, "Error! Memory not allocated. At %s, line %d. \n", __FILE__, __LINE__);
// 		exit(EXIT_FAILURE);
// 	}

// 	while (bytes > buffer_size) {
// 		if (buffer_size != fread(buffer, 1, (size_t) buffer_size, file)) {
// 			puts("Failed to read\n");
// 		}
// 		bytes -= buffer_size;
// 	}
//     BYTES rest = bytes % buffer_size;
//     if (rest > 0) {
//         if (rest != fread(buffer, sizeof(char), (size_t) rest, file)) {
//             puts("Failed to read");
//         }
//     }
// 	free(buffer);
// }

/**
 * @brief writes #(@p bytes) bytes with a buffer of @p buffer_size.

 *
 * @param fusedev
 * @param nodeid
 * @param fh
 * @param bytes
 * @param buffer_size
 */
inline void write_bytes_fuse(struct uk_fuse_dev *fusedev,
	uint64_t nodeid, uint64_t fh, BYTES bytes, BYTES buffer_size)
{
	int rc = 0;
	int iteration = 0;
	uint32_t bytes_transferred = 0;
	char *buffer = malloc(buffer_size);
	if (!buffer) {
		uk_pr_err("malloc failed\n");
		return;
	}

	while (bytes > buffer_size) {
		rc = uk_fuse_request_write(fusedev, nodeid, fh, buffer,
			buffer_size, buffer_size * iteration++,
			&bytes_transferred);
		if (rc) {
			uk_pr_err("uk_fuse_request_write has failed\n");
		}
		bytes -= buffer_size;
	}

	BYTES rest = bytes % buffer_size;
	if (rest > 0) {
		rc = uk_fuse_request_write(fusedev, nodeid, fh, buffer,
			rest, buffer_size * iteration,
			&bytes_transferred);
		if (rc) {
			uk_pr_err("uk_fuse_request_write has failed\n");
		}
	}

	free(buffer);
}

inline void write_bytes_dax(struct uk_fuse_dev, struct uk_vfdev, uint64_t nodeid,
	uint64_t fh, BYTES bytes, BYTES buffer_size)
{
	int rc
}

/*
    Initializing file names of kind: "file_0", "file_1", "file_2" and so on

*/
void init_filenames(FILES amount, int max_filename_length, char *file_names) {
	for (FILES i = 0; i < amount; i++) {
		char suffix[7 + DIGITS(i)];
		sprintf(suffix, "file_%lu", i);
		strcpy(file_names + i*max_filename_length, suffix);
	}
}