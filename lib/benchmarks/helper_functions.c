#include "uk/helper_functions.h"
#include "sys/random.h"
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


/*
	Samples natural numbers in range [lower; upper)
*/
BYTES sample_in_range(BYTES lower, BYTES upper) {
	uint32_t rand;
	getrandom(&rand, sizeof(uint32_t), 0);
	return (rand % (upper - lower)) + lower;
}

/**
 * @brief writes #(@p bytes) bytes with a buffer of @p buffer_size.
 *
 * The caller has to uk_fuse_request_open() the file.
 *
 * @param fusedev
 * @param nodeid
 * @param fh
 * @param foffset offset inside the file
 * @param bytes
 * @param buffer_size
 */
void write_bytes_fuse(struct uk_fuse_dev *fusedev, uint64_t nodeid,
		      uint64_t fh, BYTES foffset, BYTES bytes,
		      BYTES buffer_size)
{
	int rc = 0;
	int iteration = 0;
	uint32_t bytes_transferred = 0;
	char *buffer = malloc(buffer_size);
	if (unlikely(!buffer)) {
		uk_pr_err("malloc failed\n");
		return;
	}

	while (bytes > buffer_size) {
		rc = uk_fuse_request_write(fusedev, nodeid, fh, buffer,
			buffer_size, foffset + buffer_size * iteration++,
			&bytes_transferred);
		if (unlikely(rc)) {
			uk_pr_err("uk_fuse_request_write has failed\n");
		}
		bytes -= buffer_size;
	}

	BYTES rest = bytes % buffer_size;
	if (rest > 0) {
		rc = uk_fuse_request_write(fusedev, nodeid, fh, buffer,
			rest, foffset + buffer_size * iteration,
			&bytes_transferred);
		if (unlikely(rc)) {
			uk_pr_err("uk_fuse_request_write has failed\n");
		}
	}

	free(buffer);
}

/**
 * @brief
 *
 * The caller has to FUSE_SETUPMAPPING the write region of the file.
 *
 * @param fusedev
 * @param vfdev
 * @param nodeid
 * @param fh
 * @param moffset offset inside the DAX window
 * @param bytes
 * @param buffer_size
 */
void write_bytes_dax(uint64_t dax_addr, uint64_t moffset, BYTES bytes,
	BYTES buffer_size)
{
	int iteration = 0;
	BYTES rest = bytes % buffer_size;
	char *buffer = malloc(buffer_size);
	if (unlikely(!buffer)) {
		uk_pr_err("malloc failed\n");
		return;
	}

	while (bytes > buffer_size) {
		memcpy((char *) dax_addr + moffset + buffer_size * iteration++,
			buffer, buffer_size);
		bytes -= buffer_size;
	}
	if (rest) {
		memcpy((char *) dax_addr + moffset + buffer_size * iteration,
			buffer, rest);
	}

	free(buffer);
}

/*
    Reads `bytes` bytes with a 1KB buffer.
*/
void read_bytes_fuse(struct uk_fuse_dev *fusedev, uint64_t nodeid,
		     uint64_t fh, BYTES foffset, BYTES bytes, BYTES buffer_size)
{
	int rc = 0;
	int iteration = 0;
	uint32_t bytes_transferred = 0;
	char *buffer = malloc(buffer_size);
	if (unlikely(!buffer)) {
		uk_pr_err("malloc failed\n");
		return;
	}

	while (bytes > buffer_size) {
		rc = uk_fuse_request_read(fusedev, nodeid, fh,
			foffset + buffer_size * iteration++,
			buffer_size, buffer,
			&bytes_transferred);
		if (unlikely(rc)) {
			uk_pr_err("uk_fuse_request_read has failed\n");
		}
		bytes -= buffer_size;
	}

	BYTES rest = bytes % buffer_size;
	if (rest) {
		rc = uk_fuse_request_read(fusedev, nodeid, fh,
			foffset + buffer_size * iteration,
			rest, buffer,
			&bytes_transferred);
		if (unlikely(rc)) {
			uk_pr_err("uk_fuse_request_read has failed\n");
		}
	}

	free(buffer);
}

void read_bytes_dax(uint64_t dax_addr, uint64_t moffset, BYTES bytes,
	BYTES buffer_size)
{
	int iteration = 0;
	BYTES rest = bytes % buffer_size;
	char *buffer = malloc(buffer_size);
	if (unlikely(!buffer)) {
		uk_pr_err("malloc failed\n");
		return;
	}

	while (bytes > buffer_size) {
		memcpy(buffer, (char *) dax_addr + moffset
			+ buffer_size * iteration++, buffer_size);
		bytes -= buffer_size;
	}
	if (rest) {
		memcpy((char *) dax_addr + moffset + buffer_size * iteration,
			buffer, rest);
	}

	free(buffer);
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