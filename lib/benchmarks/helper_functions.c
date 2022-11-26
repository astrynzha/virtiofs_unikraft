#include "uk/helper_functions.h"
#include "sys/random.h"
#include "uk/assert.h"
#include "uk/fuse.h"
#include "uk/fusedev_core.h"
#include "uk/measurement_scenarios.h"
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

	while (bytes >= buffer_size) {
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
	// char *buffer = calloc(1, buffer_size); TODO: somehow calloc performs
	// much worse
	char *buffer = malloc(buffer_size);
	if (unlikely(!buffer)) {
		uk_pr_err("malloc failed\n");
		return;
	}

	while (bytes >= buffer_size) {
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

	while (bytes >= buffer_size) {
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

	while (bytes >= buffer_size) {
		memcpy(buffer, (char *) dax_addr + moffset
			+ buffer_size * iteration++, buffer_size);
		bytes -= buffer_size;
	}
	if (rest) {
		memcpy(buffer, (char *) dax_addr + moffset
		       + buffer_size * iteration, rest);
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

void create_all_files(struct uk_fuse_dev *fusedev, FILES *amount, size_t len,
		      int measurements)
{
	int rc;

	for (size_t i = 0; i < len; i++) {

		for (int j = 0; j < measurements; j++) {
			/* creating a directory for the j+1th measurement
			   of amount[i] files */
			fuse_file_context dcontext = {.is_dir = true,
				.mode = 0777,
				.parent_nodeid = 1
			};
			sprintf(dcontext.name, "%lu_m%d",
				amount[i], j + 1);
			rc = uk_fuse_request_mkdir(fusedev, 1,
				dcontext.name, dcontext.mode,
				&dcontext.nodeid, &dcontext.nlookup);
			if (rc) {
				uk_pr_err("uk_fuse_request_mkdir has failed \n");
				UK_CRASH("crashing...");
			}

			/* populating the directory with the amount[i] files */
			for (FILES k = 0; k < amount[i]; k++) {
				fuse_file_context fcontext = {.is_dir = false, .name = "results.csv",
					.mode = 0777, .flags = O_WRONLY | O_CREAT | O_EXCL | O_TRUNC
				};
				sprintf(fcontext.name, "%lu", k);


				rc = uk_fuse_request_create(fusedev, dcontext.nodeid, fcontext.name,
				fcontext.flags, fcontext.mode,
				&fcontext.nodeid, &fcontext.fh,
				&fcontext.nlookup);
				if (rc) {
					uk_pr_err("uk_fuse_request_create has failed \n");
					fprintf(stderr, "Error creating file number %lu.\n", i);
					UK_CRASH("crashing...");
				}
				rc = uk_fuse_request_release(fusedev,
					false, fcontext.nodeid,
					fcontext.fh);
			}
		}
	}
}

static void _fisher_yates_modern(BYTES **interval_order, BYTES total_intervals);

/**
 * @brief calculate the file intervals and the order, in which to read / write
 * them
 *
 * The caller is responsible for freeing the memory, allocated for the
 * output variables.
 *
 * @param file_size
 * @param[out] file_chunks
 * @param[out] interval_order
 * @param[out] num_intervals
 */
void slice_file(BYTES file_size, struct file_interval **intervals,
		BYTES **interval_order, BYTES *num_intervals,
		BYTES interval_len)
{
	// BYTES interval_len = MB(1);
	BYTES full_intervals = file_size / interval_len;
	BYTES total_intervals;

	if (file_size % interval_len) {
		total_intervals = full_intervals + 1;
	} else {
		total_intervals = full_intervals;
	}

	*intervals = (struct file_interval*)(0x100000000UL);
	// *intervals = malloc(sizeof(struct file_interval)
	// 	* total_intervals);
	// if (!(*intervals)) {
	// 	uk_pr_err("malloc failed \n");
	// 	UK_CRASH("malloc failed\n");
	// }
	*interval_order = (BYTES*)(((char *)(*intervals)) + (0x200000000UL));
	// *interval_order = malloc(sizeof(BYTES)
	// 	* total_intervals);
	// if (!(*interval_order)) {
	// 	uk_pr_err("malloc failed \n");
	// 	UK_CRASH("malloc failed\n");
	// }
	if (total_intervals > full_intervals) {
		((*intervals)[full_intervals]).off = interval_len * full_intervals;
		((*intervals)[full_intervals]).len = file_size % interval_len;
	}

	for (BYTES i = 0; i < full_intervals; i++) {
		((*intervals)[i]).off = interval_len * i;
		((*intervals)[i]).len = interval_len;
	}


	for (BYTES i = 0; i < total_intervals; i++) {
		(*interval_order)[i] = i;
	}

	printf("Shuffling intervals!\n");

	_fisher_yates_modern(interval_order, total_intervals);

	printf("Intervals shuffled!\n");

	*num_intervals = total_intervals;
}

/**
 * @brief calculate the file intervals and the order, in which to read / write
 * them
 *
 * The caller is responsible for freeing the memory, allocated for the
 * output variables.
 *
 * @param file_size
 * @param[out] file_chunks
 * @param[out] interval_order
 * @param[out] num_intervals
 */
void slice_file_malloc(BYTES file_size, struct file_interval **intervals,
		BYTES **interval_order, BYTES *num_intervals,
		BYTES interval_len)
{
	// BYTES interval_len = MB(1);
	BYTES full_intervals = file_size / interval_len;
	BYTES total_intervals;

	if (file_size % interval_len) {
		total_intervals = full_intervals + 1;
	} else {
		total_intervals = full_intervals;
	}

	// *intervals = (struct file_interval*)(0x100000000UL);
	*intervals = malloc(sizeof(struct file_interval)
		* total_intervals);
	if (!(*intervals)) {
		uk_pr_err("malloc failed \n");
		UK_CRASH("malloc failed\n");
	}
	// *interval_order = (BYTES*)(((char *)(*intervals)) + (0x200000000UL));
	*interval_order = malloc(sizeof(BYTES)
		* total_intervals);
	if (!(*interval_order)) {
		uk_pr_err("malloc failed \n");
		UK_CRASH("malloc failed\n");
	}
	if (total_intervals > full_intervals) {
		((*intervals)[full_intervals]).off = interval_len * full_intervals;
		((*intervals)[full_intervals]).len = file_size % interval_len;
	}

	for (BYTES i = 0; i < full_intervals; i++) {
		((*intervals)[i]).off = interval_len * i;
		((*intervals)[i]).len = interval_len;
	}


	for (BYTES i = 0; i < total_intervals; i++) {
		(*interval_order)[i] = i;
	}

	printf("Shuffling intervals!\n");

	_fisher_yates_modern(interval_order, total_intervals);

	printf("Intervals shuffled!\n");

	*num_intervals = total_intervals;
}


/**
 * Fisher-Yates algorithm that uniformly shuffles an array.
 */
static void _fisher_yates_modern(BYTES **interval_order, BYTES total_intervals)
{

// 	for i from n−1 downto 1 do
//      j ← random integer such that 0 ≤ j ≤ i
//      exchange a[j] and a[i]
	BYTES rand;
	BYTES temp;

	for (BYTES i = total_intervals - 1; i >=1; i--) {
		getrandom(&rand,sizeof(BYTES), 0);
		rand = rand % (i + 1);
		temp = (*interval_order)[rand];
		(*interval_order)[rand] = (*interval_order)[i];
		(*interval_order)[i] = temp;
	}
}