#include "uk/measurement_scenarios.h"
#include "uk/fuse.h"
#include "fcntl.h"
#include "limits.h"
#include "uk/arch/lcpu.h"
#include "uk/assert.h"
#include "uk/fuse_i.h"
#include "uk/fusedev.h"
#include "uk/fusereq.h"
#include "uk/helper_functions.h"
#include "uk/print.h"
#include "uk/time_functions.h"

#include <dirent.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* ukfuse */
#include "uk/fusedev_core.h"
#include "uk/fuse.h"
/* virtiofs */
#include "uk/vfdev.h"

/*
    Measure removing `amount` files.

    Necessary files are created and deleted by the function.
*/
__nanosec create_files(struct uk_fuse_dev *fusedev, FILES amount) {
	int rc = 0;
	fuse_file_context *fc;
	fuse_file_context dc = { .is_dir = true, .name = "create_files",
		.mode = 0777, .parent_nodeid = 1
	};
	__nanosec start = 0, end = 0;
	char *file_name;


	fc = calloc(amount, sizeof(fuse_file_context));
	if (!fc) {
		uk_pr_err("calloc has failed \n");
		return 0;
	}
	rc = uk_fuse_request_mkdir(fusedev, dc.parent_nodeid,
		dc.name, 0777, &dc.nodeid,
		&dc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_request_mkdir has failed \n");
		goto free_fc;
	}
	rc = uk_fuse_request_open(fusedev, true, dc.nodeid,
		O_RDWR, &dc.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_open (dir==true) has failed \n");
		goto free_fc;
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
		rc = uk_fuse_request_create(fusedev, dc.nodeid,
			file_name, O_WRONLY | O_CREAT | O_EXCL,
			S_IFREG | S_IRWXU | S_IRWXG | S_IRWXO,
			&fc[i].nodeid, &fc[i].fh,
			&fc[i].nlookup);
		if (unlikely(rc)) {
			uk_pr_err("uk_fuse_request_create has failed \n");
			start = 0;
			goto free_fn;
		}
		rc = uk_fuse_request_release(fusedev, false,
			fc[i].nodeid, fc[i].fh);
		if (unlikely(rc)) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			start = 0;
			goto free_fn;
		}
	}

	rc = uk_fuse_request_fsync(fusedev, true,
		dc.nodeid, dc.fh, 0);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_removemapping_legacy has failed \n");
		start = 0;
		goto free_fn;
	}

	end = _clock();

	// cleaning up: deleting all files

	for (FILES i = 0; i < amount; i++) {
		char *file_name = file_names + i * max_filename_length;
		rc = uk_fuse_request_unlink(fusedev, file_name,
			false, fc[i].nodeid,
			fc[i].nlookup, dc.nodeid);
		if (rc) {
			uk_pr_err("uk_fuse_request_unlink has failed \n");
			start = end = 0;
			goto free_fn;
		}
	}
	rc = uk_fuse_request_release(fusedev, true, dc.nodeid, dc.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_release has failed \n");
		start = end = 0;
		goto free_fn;
	}
	rc = uk_fuse_request_unlink(fusedev, dc.name, true,
		dc.nodeid, dc.nlookup, dc.parent_nodeid);
	if (rc) {
		uk_pr_err("uk_fuse_request_unlink has failed \n");
		start = end = 0;
		goto free_fn;
	}

free_fn:
	free(file_names);
free_fc:
	free(fc);
	return end - start;
}

/*
    Measure creating `amount` files.

    Necessary files are created and deleted by the function.
*/
__nanosec remove_files(struct uk_fuse_dev *fusedev, FILES amount,
		       int measurement) {
	int rc = 0;
	fuse_file_context *fc;
	__nanosec start = 0, end = 0;


	fc = calloc(amount, sizeof(fuse_file_context));
	if (!fc) {
		uk_pr_err("calloc has failed \n");
		return 0;
	}

	// experiment directory name
	fuse_file_context dc;
	sprintf(dc.name, "%lu_m%d", amount, measurement);

	// looking up & opening the experiment directory
	rc = uk_fuse_request_lookup(fusedev, 1,
		dc.name, &dc.nodeid);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_lookup has failed \n");
	}

	rc = uk_fuse_request_open(fusedev, true, dc.nodeid,
		O_RDONLY, &dc.fh);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_open has failed \n");
		goto free_fc;
	}

	// experiment file names

	char fname[256];
	for (FILES i = 0; i < amount; i++) {
		sprintf(fname, "%lu", i);
		strncpy(fc[i].name, fname, 256);
	}


	// looking up experiment files
	for (FILES i = 0; i < amount; i++) {
		rc = uk_fuse_request_lookup(fusedev, dc.nodeid,
			fc[i].name, &fc[i].nodeid);
		if (unlikely(rc)) {
			uk_pr_err("uk_fuse_request_lookup has failed \n");
		}
	}

	// measuring the deletion of `amount` files

	start = _clock();

	for (FILES i = 0; i < amount; i++) {
		rc = uk_fuse_request_unlink(fusedev, fc[i].name,
			false, fc[i].nodeid,
			0, dc.nodeid);
		if (unlikely(rc)) {
			uk_pr_err("uk_fuse_request_unlink has failed \n");
			goto free_fc;
		}
	}

	rc = uk_fuse_request_fsync(fusedev, true,
		dc.nodeid, dc.fh, 0);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_removemapping_legacy has failed \n");
		start = 0;
		goto free_fc;
	}

	end = _clock();

	rc = uk_fuse_request_release(fusedev, true, dc.nodeid, dc.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_release has failed \n");
		start = end = 0;
		goto free_fc;
	}
	rc = uk_fuse_request_forget(fusedev, dc.nodeid, 0);
	if (rc) {
		uk_pr_err("uk_fuse_request_forget has failed \n");
		goto free_fc;
	}

	free(fc);
	return end - start;

free_fc:
	free(fc);
	return 0;
}

/*
*/

/**
 * @brief
 *
 * Measure listing (e.g. ls command) of files. 'file_amount'
 * specifies how many files are in the directory.
 *
 * Necessary files are to be created and deleted by the caller
 *
 * @param fusedev
 * @param file_amount
 * @param parent nodeid of the directory, where the files are located
 * @return __nanosec
 */
__nanosec list_dir(struct uk_fuse_dev *fusedev, FILES file_amount, int measurement) {
	int rc = 0;
	struct fuse_dirent *dirents;
	size_t num_dirents;
	__nanosec start = 0, end = 0;

	dirents = calloc(file_amount, sizeof(struct fuse_dirent));
	if (!dirents) {
		uk_pr_err("calloc failed \n");
		return 0;
	}

	if (file_amount == 131072) {
		uk_pr_info("hi\n");
	}

	fuse_file_context dc;
	sprintf(dc.name, "%lu_m%d", file_amount, measurement);

	rc = uk_fuse_request_lookup(fusedev, 1,
		dc.name, &dc.nodeid);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_lookup has failed \n");
	}

	rc = uk_fuse_request_open(fusedev, true, dc.nodeid,
		O_RDONLY, &dc.fh);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_open has failed \n");
		goto free;
	}

	start = _clock();

	rc = uk_fuse_request_readdirplus(fusedev, 4096,
		dc.nodeid, dc.fh, dirents, &num_dirents);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_readdirplus has failed \n");
		goto free;
	}

	end = _clock();

	UK_ASSERT(file_amount + 2 == num_dirents);

	rc = uk_fuse_request_release(fusedev, true,
		dc.nodeid, dc.fh);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_release has failed \n");
		goto free;
	}

	rc = uk_fuse_request_forget(fusedev, dc.nodeid, 0);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_forget has failed \n");
		goto free;
	}

	free(dirents);
	return end - start;

free:
	free(dirents);
	return 0;
}

/*
	Measuring sequential write with buffer on heap, allocated with malloc.
	Buffer size can be set through 'buffer_size'.

	Write file is created and deleted by the function.
*/
__nanosec write_seq_fuse(struct uk_fuse_dev *fusedev, BYTES bytes, BYTES buffer_size)
{
	__nanosec start, end;
	int rc = 0;
	fuse_file_context file = {
		.is_dir = false, .name = "1G_file",
		.flags = O_WRONLY,
		.parent_nodeid = 1,
	};
	uint32_t bytes_transferred = 0;
	char *buffer = malloc(buffer_size);
	if (buffer == NULL) {
		uk_pr_err("malloc failed\n");
		return 0;
	}
	memset(buffer, '1', buffer_size);
	buffer[buffer_size - 1] = '\0';
	BYTES iterations = bytes / buffer_size;
	BYTES rest = bytes % buffer_size;

	rc = uk_fuse_request_lookup(fusedev, 1, file.name,
		&file.nodeid);
	if (rc) {
		uk_pr_err("uk_fuse_request_lookup has failed \n");
		start = end = 0;
		goto free;
	}
	rc = uk_fuse_request_open(fusedev, false, file.nodeid,
		file.flags, &file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_open has failed \n");
		start = end = 0;
		goto free;
	}

	start = _clock();

	for (BYTES i = 0; i < iterations; i++) {
		rc = uk_fuse_request_write(fusedev, file.nodeid,
			file.fh, buffer, buffer_size,
			buffer_size*i, &bytes_transferred);
		if (unlikely(rc)) {
			uk_pr_err("uk_fuse_request_write has failed \n");
			goto free;
		}
	}
	if (rest > 0) {
		rc = uk_fuse_request_write(fusedev, file.nodeid,
			file.fh, buffer, rest,
			buffer_size*iterations, &bytes_transferred);
		if (unlikely(rc)) {
			uk_pr_err("uk_fuse_request_write has failed \n");
			goto free;
		}
	}

	rc = uk_fuse_request_fsync(fusedev, false, file.nodeid,
		file.fh, 0);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_fsync has failed\n");
		goto free;
	}

	end = _clock();

	rc = uk_fuse_request_release(fusedev, false,
		file.nodeid, file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_release has failed \n");
		goto free;
	}

	rc = uk_fuse_request_forget(fusedev, file.nodeid, 1);
	if (rc) {
			uk_pr_err("uk_fuse_request_forget has failed \n");
			start = end = 0;
			goto free;
	}

	free(buffer);
	return end - start;

free:
	free(buffer);
	return 0;
}

/**
 * @brief
 *
 * Requires a file called "1G_file" of size 1GiB in the root directory of
 * the shared file system.
 *
 * @param fusedev
 * @param bytes
 * @param buffer_size
 * @param dir
 * @return __nanosec
 */
__nanosec write_seq_dax(struct uk_fuse_dev *fusedev, struct uk_vfdev *vfdev,
			BYTES bytes, BYTES buffer_size)
{
	int rc = 0;
	fuse_file_context file = {
		.is_dir = false, .name = "1G_file",
		.flags = O_WRONLY,
		.parent_nodeid = 1,
	};
	char *buffer = malloc(buffer_size);
	if (buffer == NULL) {
		uk_pr_err("malloc failed\n");
		return 0;
	}
	memset(buffer, '1', buffer_size);
	buffer[buffer_size - 1] = '\0';
	BYTES iterations = bytes / buffer_size;
	BYTES rest = bytes % buffer_size;
	uint64_t dax_addr = vfdev->dax_addr;

	rc = uk_fuse_request_lookup(fusedev, 1, file.name,
		&file.nodeid);
	if (rc) {
		uk_pr_err("uk_fuse_request_lookup has failed \n");
		goto free;
	}
	rc = uk_fuse_request_open(fusedev, false, file.nodeid,
		file.flags, &file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_open has failed \n");
		goto free;
	}

	rc = uk_fuse_request_setupmapping(fusedev, file.nodeid,
		file.fh, 0, bytes,
		FUSE_SETUPMAPPING_FLAG_WRITE, 0);
	if (rc) {
		uk_pr_err("uk_fuse_request_setupmapping has failed \n");
		goto free;
	}

	__nanosec start, end;
	start = _clock();

	for (BYTES i = 0; i < iterations; i++) {
		memcpy(((char *) dax_addr) + buffer_size*i, buffer,
			buffer_size);
	}
	if (rest > 0) {
		memcpy(((char *) dax_addr) + buffer_size*iterations, buffer,
			rest);
	}

	rc = uk_fuse_request_fsync(fusedev, false, file.nodeid,
		file.fh, 0);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_fsync has failed\n");
		goto free;
	}

	end = _clock();

	rc = uk_fuse_request_removemapping_legacy(fusedev, file.nodeid,
		file.fh, 0, bytes);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_removemapping_legacy has failed \n");
		start = end = 0;
		goto free;
	}

	rc = uk_fuse_request_release(fusedev, false,
		file.nodeid, file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_release has failed \n");
		start = end = 0;
		goto free;
	}

	rc = uk_fuse_request_forget(fusedev, file.nodeid, 1);
	if (rc) {
		uk_pr_err("uk_fuse_request_forget has failed \n");
		start = end = 0;
		goto free;
	}

	free(buffer);
	return end - start;

free:
	free(buffer);
	return 0;
}

/**
 * @brief
 *
 * Requires a file called "1G_file" of size 1GiB in the root directory of
 * the shared file system.
 *
 * @param fusedev
 * @param bytes
 * @param buffer_size
 * @param dir
 * @return __nanosec
 */
__nanosec write_seq_dax_2(struct uk_fuse_dev *fusedev, struct uk_vfdev *vfdev,
			BYTES bytes, BYTES buffer_size)
{
	int rc = 0;
	fuse_file_context file = {
		.is_dir = false, .name = "1G_file",
		.flags = O_WRONLY,
		.parent_nodeid = 1,
	};
	char *buffer = malloc(buffer_size);
	if (buffer == NULL) {
		uk_pr_err("malloc failed\n");
		return 0;
	}
	memset(buffer, '1', buffer_size);
	buffer[buffer_size - 1] = '\0';
	BYTES iterations = bytes / buffer_size;
	BYTES rest = bytes % buffer_size;
	uint64_t dax_addr = vfdev->dax_addr;

	rc = uk_fuse_request_lookup(fusedev, 1, file.name,
		&file.nodeid);
	if (rc) {
		uk_pr_err("uk_fuse_request_lookup has failed \n");
		goto free;
	}
	rc = uk_fuse_request_open(fusedev, false, file.nodeid,
		file.flags, &file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_open has failed \n");
		goto free;
	}

	rc = uk_fuse_request_setupmapping(fusedev, file.nodeid,
		file.fh, 0, bytes,
		FUSE_SETUPMAPPING_FLAG_WRITE, 0);
	if (rc) {
		uk_pr_err("uk_fuse_request_setupmapping has failed \n");
		goto free;
	}

	__nanosec start, end;
	/* Write for the first time */
	printf("Running for the first time:\n");

	start = _clock();

	for (BYTES i = 0; i < iterations; i++) {
		memcpy(((char *) dax_addr) + buffer_size*i, buffer,
			buffer_size);
	}
	if (rest > 0) {
		memcpy(((char *) dax_addr) + buffer_size*iterations, buffer,
			rest);
	}

	rc = uk_fuse_request_fsync(fusedev, false, file.nodeid,
		file.fh, 0);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_fsync has failed\n");
		goto free;
	}

	end = _clock();

	__nanosec result_ms = nanosec_to_milisec(end - start);
	printf("Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);
	printf("Running for the second time:\n");

	/* Write for the second time */

	start = _clock();

	for (BYTES i = 0; i < iterations; i++) {
		memcpy(((char *) dax_addr) + buffer_size*i, buffer,
			buffer_size);
	}
	if (rest > 0) {
		memcpy(((char *) dax_addr) + buffer_size*iterations, buffer,
			rest);
	}

	rc = uk_fuse_request_fsync(fusedev, false, file.nodeid,
		file.fh, 0);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_fsync has failed\n");
		goto free;
	}

	end = _clock();

	rc = uk_fuse_request_removemapping_legacy(fusedev, file.nodeid,
		file.fh, 0, bytes);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_removemapping_legacy has failed \n");
		start = end = 0;
		goto free;
	}

	rc = uk_fuse_request_release(fusedev, false,
		file.nodeid, file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_release has failed \n");
		start = end = 0;
		goto free;
	}

	rc = uk_fuse_request_forget(fusedev, file.nodeid, 1);
	if (rc) {
		uk_pr_err("uk_fuse_request_forget has failed \n");
		start = end = 0;
		goto free;
	}

	free(buffer);
	return end - start;

free:
	free(buffer);
	return 0;
}

/*
	Measuring random access write (non-sequential) of an existing file,
	passed to the function.
	Seed has to be set by the caller.


	The function:
	1. Randomly determines a file position from range
	[0, EOF - upper_write_limit).
	2. Writes a random amount of bytes, sampled from range
	[lower_write_limit, upper_write_limit].
	3. Repeats steps 1-2 until the 'remaining_bytes' amount of bytes
	is written.
*/

/**
 * @brief
 *
 * Assumes a file called "1G_file" of size 100GiB exists in the root
 * directory.
 *
 * @param fusedev
 * @param remaining_bytes
 * @param buffer_size
 * @param lower_write_limit
 * @param upper_write_limit
 * @return __nanosec
 */
__nanosec write_randomly_fuse(struct uk_fuse_dev *fusedev,
			      BYTES bytes, BYTES buffer_size,
			      BYTES interval_len)
{
	__nanosec start, end;
	int rc = 0;
	fuse_file_context file = {
		.is_dir = false, .name = "1G_file",
		.flags = O_WRONLY,
		.parent_nodeid = 1,
	};

	struct file_interval *intervals;
	BYTES *interval_order;
	BYTES num_intervals;

	slice_file_malloc(bytes, &intervals,
		&interval_order, &num_intervals,
		interval_len);

	rc = uk_fuse_request_lookup(fusedev, 1, file.name,
		&file.nodeid);
	if (rc) {
		uk_pr_err("uk_fuse_request_lookup has failed \n");
		start = end = 0;
		goto out;
	}
	rc = uk_fuse_request_open(fusedev, false, file.nodeid,
		file.flags, &file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_open has failed \n");
		start = end = 0;
		goto out;
	}

	BYTES off;
	BYTES len;
	uint32_t bytes_transferred = 0;
	char *buffer = malloc(buffer_size);
	if (unlikely(!buffer)) {
		uk_pr_err("malloc failed\n");
		start = end = 0;
		goto out;
	}
	memset(buffer, '1', buffer_size);
	buffer[buffer_size - 1] = '\0';

	start = _clock();

	for (BYTES i = 0; i < num_intervals; i++) {
		off = intervals[interval_order[i]].off;
		len = intervals[interval_order[i]].len;

		BYTES iterations = len / buffer_size;
		BYTES rest = len % buffer_size;

		for (BYTES j = 0; j < iterations; j++) {
			rc = uk_fuse_request_write(fusedev, file.nodeid, file.fh,
				buffer,
				buffer_size, off + buffer_size * j,
				&bytes_transferred);
			if (unlikely(rc)) {
				uk_pr_err("uk_fuse_request_read has failed\n");
			}
		}
		if (rest > 0) {
			rc = uk_fuse_request_write(fusedev, file.nodeid,
			file.fh, buffer,
			rest, off + buffer_size * iterations,
			&bytes_transferred);
			if (unlikely(rc)) {
				uk_pr_err("uk_fuse_request_read has failed\n");
			}
		}
	}

	rc = uk_fuse_request_fsync(fusedev, false, file.nodeid,
		file.fh, 0);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_fsync has failed\n");
		start = end = 0;
		goto out1;
	}

	end = _clock();

	rc = uk_fuse_request_release(fusedev, false,
		file.nodeid, file.fh);
	if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			start = end = 0;
			goto out1;
	}
	rc = uk_fuse_request_forget(fusedev, file.nodeid, 1);
	if (rc) {
			uk_pr_err("uk_fuse_request_forget has failed \n");
			start = end = 0;
			goto out1;
	}

out1:
	free(buffer);
out:
	free(interval_order);
	free(intervals);
	return end - start;
}

__nanosec write_randomly_dax(struct uk_fuse_dev *fusedev,
			     struct uk_vfdev *vfdev, BYTES bytes,
			     BYTES buffer_size, BYTES interval_len)
{
	__nanosec start, end;
	int rc = 0;
	fuse_file_context file = {
		.is_dir = false, .name = "1G_file",
		.flags = O_WRONLY,
		.parent_nodeid = 1,
	};

	struct file_interval *intervals;
	BYTES *interval_order;
	BYTES num_intervals;

	slice_file(bytes, &intervals,
		&interval_order, &num_intervals, interval_len);

	rc = uk_fuse_request_lookup(fusedev, 1, file.name,
		&file.nodeid);
	if (rc) {
		uk_pr_err("uk_fuse_request_lookup has failed \n");
		start = end = 0;
		goto out;
	}
	rc = uk_fuse_request_open(fusedev, false, file.nodeid,
		file.flags, &file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_open has failed \n");
		start = end = 0;
		goto out;
	}

	rc = uk_fuse_request_setupmapping(fusedev, file.nodeid,
		file.fh, 0, bytes,
		FUSE_SETUPMAPPING_FLAG_WRITE, 0);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_setupmapping has failed \n");
		start = end = 0;
		goto out;
	}

	BYTES off;
	BYTES len;
	char *buffer = malloc(buffer_size);
	if (unlikely(!buffer)) {
		uk_pr_err("malloc failed\n");
		start = end = 0;
		goto out;
	}
	memset(buffer, '1', buffer_size);
	buffer[buffer_size - 1] = '\0';

	start = _clock();

	for (BYTES i = 0; i < num_intervals; i++) {
		off = intervals[interval_order[i]].off;
		len = intervals[interval_order[i]].len;

		BYTES iterations = len / buffer_size;
		BYTES rest = len % buffer_size;

		for (BYTES j = 0; j < iterations; j++) {
			memcpy((char *)vfdev->dax_addr + off
				+ buffer_size * j,
				buffer, buffer_size);
		}
		if (rest > 0) {
			memcpy((char *)vfdev->dax_addr + off
				+ buffer_size * iterations,
				buffer, rest);
		}

		// write_bytes_dax(vfdev->dax_addr,
		// 	intervals[interval_order[i]].off,
		// 	intervals[interval_order[i]].len,
		// 	buffer_size);
	}

	rc = uk_fuse_request_fsync(fusedev, false, file.nodeid,
		file.fh, 0);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_fsync has failed\n");
		start = end = 0;
		goto out1;
	}

	end = _clock();

	rc = uk_fuse_request_removemapping_legacy(fusedev, file.nodeid,
		file.fh, 0, bytes);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_removemapping_legacy has failed \n");
		start = end = 0;
		goto out1;
	}

	rc = uk_fuse_request_release(fusedev, false,
		file.nodeid, file.fh);
	if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			start = end = 0;
			goto out1;
	}
	rc = uk_fuse_request_forget(fusedev, file.nodeid, 1);
	if (rc) {
			uk_pr_err("uk_fuse_request_forget has failed \n");
			start = end = 0;
			goto out1;
	}

out1:
	free(buffer);
out:
	return end - start;
}


__nanosec write_randomly_dax_2(struct uk_fuse_dev *fusedev,
			     struct uk_vfdev *vfdev, BYTES bytes,
			     BYTES buffer_size, BYTES interval_len)
{
	__nanosec start, end;
	int rc = 0;
	fuse_file_context file = {
		.is_dir = false, .name = "1G_file",
		.flags = O_WRONLY,
		.parent_nodeid = 1,
	};

	struct file_interval *intervals;
	BYTES *interval_order;
	BYTES num_intervals;

	slice_file(bytes, &intervals,
		&interval_order, &num_intervals, interval_len);

	rc = uk_fuse_request_lookup(fusedev, 1, file.name,
		&file.nodeid);
	if (rc) {
		uk_pr_err("uk_fuse_request_lookup has failed \n");
		start = end = 0;
		goto out;
	}
	rc = uk_fuse_request_open(fusedev, false, file.nodeid,
		file.flags, &file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_open has failed \n");
		start = end = 0;
		goto out;
	}

	rc = uk_fuse_request_setupmapping(fusedev, file.nodeid,
		file.fh, 0, bytes,
		FUSE_SETUPMAPPING_FLAG_WRITE, 0);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_setupmapping has failed \n");
		start = end = 0;
		goto out;
	}

	BYTES off;
	BYTES len;
	char *buffer = malloc(buffer_size);
	if (unlikely(!buffer)) {
		uk_pr_err("malloc failed\n");
		start = end = 0;
		goto out;
	}
	memset(buffer, '1', buffer_size);
	buffer[buffer_size - 1] = '\0';

	/* Write for the first time */

	printf("Running for the first time:\n");
	start = _clock();

	for (BYTES i = 0; i < num_intervals; i++) {
		off = intervals[interval_order[i]].off;
		len = intervals[interval_order[i]].len;

		BYTES iterations = len / buffer_size;
		BYTES rest = len % buffer_size;

		for (BYTES j = 0; j < iterations; j++) {
			memcpy((char *)vfdev->dax_addr + off
				+ buffer_size * j,
				buffer, buffer_size);
		}
		if (rest > 0) {
			memcpy((char *)vfdev->dax_addr + off
				+ buffer_size * iterations,
				buffer, rest);
		}
	}

	rc = uk_fuse_request_fsync(fusedev, false, file.nodeid,
		file.fh, 0);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_fsync has failed\n");
		start = end = 0;
		goto out1;
	}

	end = _clock();

	__nanosec result_ms = nanosec_to_milisec(end - start);
	printf("Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);
	printf("Running for the second time:\n");

	/* Write for the second time */

	start = _clock();

	for (BYTES i = 0; i < num_intervals; i++) {
		off = intervals[interval_order[i]].off;
		len = intervals[interval_order[i]].len;

		BYTES iterations = len / buffer_size;
		BYTES rest = len % buffer_size;

		for (BYTES j = 0; j < iterations; j++) {
			memcpy((char *)vfdev->dax_addr + off
				+ buffer_size * j,
				buffer, buffer_size);
		}
		if (rest > 0) {
			memcpy((char *)vfdev->dax_addr + off
				+ buffer_size * iterations,
				buffer, rest);
		}
	}

	rc = uk_fuse_request_fsync(fusedev, false, file.nodeid,
		file.fh, 0);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_fsync has failed\n");
		start = end = 0;
		goto out1;
	}

	end = _clock();

	rc = uk_fuse_request_removemapping_legacy(fusedev, file.nodeid,
		file.fh, 0, bytes);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_removemapping_legacy has failed \n");
		start = end = 0;
		goto out1;
	}

	rc = uk_fuse_request_release(fusedev, false,
		file.nodeid, file.fh);
	if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			start = end = 0;
			goto out1;
	}
	rc = uk_fuse_request_forget(fusedev, file.nodeid, 1);
	if (rc) {
			uk_pr_err("uk_fuse_request_forget has failed \n");
			start = end = 0;
			goto out1;
	}

out1:
	free(buffer);
out:
	return end - start;
}



/*
    Measure sequential read of `bytes` bytes.
*/
__nanosec read_seq_fuse(struct uk_fuse_dev *fusedev, BYTES bytes,
			BYTES buffer_size)
{
	int rc = 0;
	fuse_file_context file = {
		.is_dir = false, .name = "1G_file",
		.flags = O_RDONLY,
		.parent_nodeid = 1
	};
	uint32_t bytes_transferred = 0;
	char *buffer = malloc(buffer_size);
	if (buffer == NULL) {
		uk_pr_err("malloc failed\n");
		return 0;
	}

	BYTES iterations = bytes / buffer_size;
	BYTES rest = bytes % buffer_size;

	rc = uk_fuse_request_lookup(fusedev, 1, file.name,
		&file.nodeid);
	if (rc) {
		uk_pr_err("uk_fuse_request_lookup has failed \n");
		goto free;
	}
	rc = uk_fuse_request_open(fusedev, false, file.nodeid,
		file.flags, &file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_open has failed \n");
		goto free;
	}

	__nanosec start, end;

	start = _clock();

	for (BYTES i = 0; i < iterations; i++) {
		rc = uk_fuse_request_read(fusedev, file.nodeid,
			file.fh, buffer_size*i, buffer_size,
			buffer, &bytes_transferred);
		if (unlikely(rc)) {
			uk_pr_err("uk_fuse_request_read has failed \n");
			goto free;
		}
	}
	if (rest > 0) {
		rc = uk_fuse_request_read(fusedev, file.nodeid,
			file.fh, buffer_size*iterations, rest,
			buffer, &bytes_transferred);
		if (unlikely(rc)) {
			uk_pr_err("uk_fuse_request_read has failed \n");
			goto free;
		}
	}

	end = _clock();

	rc = uk_fuse_request_release(fusedev, false,
		file.nodeid, file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_release has failed \n");
		goto free;
	}

	rc = uk_fuse_request_forget(fusedev, file.nodeid, 1);
	if (rc) {
		uk_pr_err("uk_fuse_request_forget has failed \n");
		goto free;
	}

	free(buffer);
	return end - start;

free:
	free(buffer);
	return 0;
}

__nanosec read_seq_dax(struct uk_fuse_dev *fusedev, struct uk_vfdev *vfdev,
		       BYTES bytes, BYTES buffer_size)
{
	int rc = 0;
	fuse_file_context file = {
		.is_dir = false, .name = "1G_file",
		.flags = O_RDONLY,
		.parent_nodeid = 1
	};
	char *buffer = malloc(buffer_size);
	if (buffer == NULL) {
		uk_pr_err("malloc failed\n");
		return 0;
	}
	memset(buffer, '1', buffer_size);
	buffer[buffer_size - 1] = '\0';
	BYTES iterations = bytes / buffer_size;
	BYTES rest = bytes % buffer_size;
	uint64_t dax_addr = vfdev->dax_addr;

	rc = uk_fuse_request_lookup(fusedev, 1, file.name,
		&file.nodeid);
	if (rc) {
		uk_pr_err("uk_fuse_request_lookup has failed \n");
		goto free;
	}
	rc = uk_fuse_request_open(fusedev, false, file.nodeid,
		file.flags, &file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_open has failed \n");
		goto free;
	}

	rc = uk_fuse_request_setupmapping(fusedev, file.nodeid,
		file.fh, 0, bytes,
		FUSE_SETUPMAPPING_FLAG_READ, 0);
	if (rc) {
		uk_pr_err("uk_fuse_request_setupmapping has failed \n");
		goto free;
	}

	__nanosec start, end;

	start = _clock();

	for (BYTES i = 0; i < iterations; i++) {
		memcpy(buffer, ((char *) dax_addr) + buffer_size * i,
			buffer_size);
	}
	if (rest > 0) {
		memcpy(buffer, ((char *) dax_addr) + buffer_size * iterations,
			rest);
	}

	end = _clock();

	rc = uk_fuse_request_removemapping_legacy(fusedev, file.nodeid,
		file.fh, 0, bytes);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_removemapping_legacy has failed \n");
		start = end = 0;
		goto free;
	}


	rc = uk_fuse_request_release(fusedev, false,
		file.nodeid, file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_release has failed \n");
		start = end = 0;
		goto free;
	}

	rc = uk_fuse_request_forget(fusedev, file.nodeid, 1);
	if (rc) {
		uk_pr_err("uk_fuse_request_forget has failed \n");
		start = end = 0;
		goto free;
	}

	free(buffer);
	return end - start;

free:
	free(buffer);
	return 0;
}


__nanosec read_seq_dax_2(struct uk_fuse_dev *fusedev, struct uk_vfdev *vfdev,
		       BYTES bytes, BYTES buffer_size)
{
	int rc = 0;
	fuse_file_context file = {
		.is_dir = false, .name = "1G_file",
		.flags = O_RDONLY,
		.parent_nodeid = 1
	};
	char *buffer = malloc(buffer_size);
	if (buffer == NULL) {
		uk_pr_err("malloc failed\n");
		return 0;
	}
	memset(buffer, '1', buffer_size);
	buffer[buffer_size - 1] = '\0';
	BYTES iterations = bytes / buffer_size;
	BYTES rest = bytes % buffer_size;
	uint64_t dax_addr = vfdev->dax_addr;

	rc = uk_fuse_request_lookup(fusedev, 1, file.name,
		&file.nodeid);
	if (rc) {
		uk_pr_err("uk_fuse_request_lookup has failed \n");
		goto free;
	}
	rc = uk_fuse_request_open(fusedev, false, file.nodeid,
		file.flags, &file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_open has failed \n");
		goto free;
	}

	rc = uk_fuse_request_setupmapping(fusedev, file.nodeid,
		file.fh, 0, bytes,
		FUSE_SETUPMAPPING_FLAG_READ, 0);
	if (rc) {
		uk_pr_err("uk_fuse_request_setupmapping has failed \n");
		goto free;
	}

	__nanosec start, end;

	/* Write for the first time */

	printf("Running for the first time:\n");
	start = _clock();

	for (BYTES i = 0; i < iterations; i++) {
		memcpy(buffer, ((char *) dax_addr) + buffer_size * i,
			buffer_size);
	}
	if (rest > 0) {
		memcpy(buffer, ((char *) dax_addr) + buffer_size * iterations,
			rest);
	}

	end = _clock();

	__nanosec result_ms = nanosec_to_milisec(end - start);
	printf("Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);
	printf("Running for the second time:\n");
	/* Write for the second time */

	start = _clock();

	for (BYTES i = 0; i < iterations; i++) {
		memcpy(buffer, ((char *) dax_addr) + buffer_size * i,
			buffer_size);
	}
	if (rest > 0) {
		memcpy(buffer, ((char *) dax_addr) + buffer_size * iterations,
			rest);
	}

	end = _clock();

	rc = uk_fuse_request_removemapping_legacy(fusedev, file.nodeid,
		file.fh, 0, bytes);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_removemapping_legacy has failed \n");
		start = end = 0;
		goto free;
	}


	rc = uk_fuse_request_release(fusedev, false,
		file.nodeid, file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_release has failed \n");
		start = end = 0;
		goto free;
	}

	rc = uk_fuse_request_forget(fusedev, file.nodeid, 1);
	if (rc) {
		uk_pr_err("uk_fuse_request_forget has failed \n");
		start = end = 0;
		goto free;
	}

	free(buffer);
	return end - start;

free:
	free(buffer);
	return 0;
}

__nanosec read_randomly_fuse(struct uk_fuse_dev *fusedev,
			     BYTES size, BYTES buffer_size,
			     BYTES interval_len)
{
	__nanosec start, end;
	int rc = 0;
	fuse_file_context file = {
		.is_dir = false, .name = "1G_file",
		.flags = O_RDONLY,
		.parent_nodeid = 1,
	};

	struct file_interval *intervals;
	BYTES *interval_order;
	BYTES num_intervals;

	slice_file_malloc(size, &intervals,
		&interval_order, &num_intervals, interval_len);

	rc = uk_fuse_request_lookup(fusedev, 1, file.name,
		&file.nodeid);
	if (rc) {
		uk_pr_err("uk_fuse_request_lookup has failed \n");
		start = end = 0;
		goto out;
	}
	rc = uk_fuse_request_open(fusedev, false, file.nodeid,
		file.flags, &file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_open has failed \n");
		start = end = 0;
		goto out;
	}

	BYTES off;
	BYTES len;
	uint32_t bytes_transferred = 0;
	char *buffer = malloc(buffer_size);
	if (unlikely(!buffer)) {
		uk_pr_err("malloc failed\n");
		start = end = 0;
		goto out;
	}
	memset(buffer, '1', buffer_size);
	buffer[buffer_size - 1] = '\0';

	start = _clock();

	for (BYTES i = 0; i < num_intervals; i++) {
		off = intervals[interval_order[i]].off;
		len = intervals[interval_order[i]].len;

		BYTES iterations = len / buffer_size;
		BYTES rest = len % buffer_size;

		for (BYTES j = 0; j < iterations; j++) {
			rc = uk_fuse_request_read(fusedev, file.nodeid, file.fh,
				off + buffer_size * j,
				buffer_size, buffer,
				&bytes_transferred);
			if (unlikely(rc)) {
				uk_pr_err("uk_fuse_request_read has failed\n");
			}
		}
		if (rest > 0) {
			rc = uk_fuse_request_read(fusedev, file.nodeid, file.fh,
				off + buffer_size * iterations,
				rest, buffer,
				&bytes_transferred);
			if (unlikely(rc)) {
				uk_pr_err("uk_fuse_request_read has failed\n");
			}
		}
	}

	end = _clock();

	rc = uk_fuse_request_release(fusedev, false,
		file.nodeid, file.fh);
	if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			start = end = 0;
			goto out1;
	}
	rc = uk_fuse_request_forget(fusedev, file.nodeid, 1);
	if (rc) {
			uk_pr_err("uk_fuse_request_forget has failed \n");
			start = end = 0;
			goto out1;
	}

out1:
	free(buffer);
out:
	free(interval_order);
	free(intervals);
	return end - start;
}

__nanosec read_randomly_dax(struct uk_fuse_dev *fusedev,
			    struct uk_vfdev *vfdev, BYTES size,
			    BYTES buffer_size, BYTES interval_len)
{
	__nanosec start, end;
	int rc = 0;
	fuse_file_context file = {
		.is_dir = false, .name = "1G_file",
		.flags = O_RDONLY,
		.parent_nodeid = 1,
	};

	struct file_interval *intervals;
	BYTES *interval_order;
	BYTES num_intervals;

	slice_file(size, &intervals,
		&interval_order, &num_intervals, interval_len);

	rc = uk_fuse_request_lookup(fusedev, 1, file.name,
		&file.nodeid);
	if (rc) {
		uk_pr_err("uk_fuse_request_lookup has failed \n");
		start = end = 0;
		goto out;
	}
	rc = uk_fuse_request_open(fusedev, false, file.nodeid,
		file.flags, &file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_open has failed \n");
		start = end = 0;
		goto out;
	}

	rc = uk_fuse_request_setupmapping(fusedev, file.nodeid,
		file.fh, 0, size,
		FUSE_SETUPMAPPING_FLAG_READ, 0);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_setupmapping has failed \n");
		start = end = 0;
		goto out;
	}

	BYTES off;
	BYTES len;
	char *buffer = malloc(buffer_size);
	if (unlikely(!buffer)) {
		uk_pr_err("malloc failed\n");
		start = end = 0;
		goto out;
	}
	memset(buffer, '1', buffer_size);
	buffer[buffer_size - 1] = '\0';

	start = _clock();

	for (BYTES i = 0; i < num_intervals; i++) {
		off = intervals[interval_order[i]].off;
		len = intervals[interval_order[i]].len;

		BYTES iterations = len / buffer_size;
		BYTES rest = len % buffer_size;

		for (BYTES j = 0; j < iterations; j++) {
			memcpy(buffer, (char *)vfdev->dax_addr + off
				+ buffer_size * j,
				buffer_size);
		}
		if (rest > 0) {
			memcpy(buffer, (char *)vfdev->dax_addr + off
				+ buffer_size * iterations,
				rest);
		}
	}

	end = _clock();

	rc = uk_fuse_request_removemapping_legacy(fusedev, file.nodeid,
		file.fh, 0, size);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_removemapping_legacy has failed \n");
		start = end = 0;
		goto out1;
	}

	rc = uk_fuse_request_release(fusedev, false,
		file.nodeid, file.fh);
	if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			start = end = 0;
			goto out1;
	}
	rc = uk_fuse_request_forget(fusedev, file.nodeid, 1);
	if (rc) {
			uk_pr_err("uk_fuse_request_forget has failed \n");
			start = end = 0;
			goto out1;
	}

out1:
	free(buffer);
out:
	return end - start;
}

__nanosec read_randomly_dax_2(struct uk_fuse_dev *fusedev,
			    struct uk_vfdev *vfdev, BYTES size,
			    BYTES buffer_size, BYTES interval_len)
{
	__nanosec start, end;
	int rc = 0;
	fuse_file_context file = {
		.is_dir = false, .name = "1G_file",
		.flags = O_RDONLY,
		.parent_nodeid = 1,
	};

	struct file_interval *intervals;
	BYTES *interval_order;
	BYTES num_intervals;

	slice_file(size, &intervals,
		&interval_order, &num_intervals, interval_len);

	rc = uk_fuse_request_lookup(fusedev, 1, file.name,
		&file.nodeid);
	if (rc) {
		uk_pr_err("uk_fuse_request_lookup has failed \n");
		start = end = 0;
		goto out;
	}
	rc = uk_fuse_request_open(fusedev, false, file.nodeid,
		file.flags, &file.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_open has failed \n");
		start = end = 0;
		goto out;
	}

	rc = uk_fuse_request_setupmapping(fusedev, file.nodeid,
		file.fh, 0, size,
		FUSE_SETUPMAPPING_FLAG_READ, 0);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_setupmapping has failed \n");
		start = end = 0;
		goto out;
	}

	BYTES off;
	BYTES len;
	char *buffer = malloc(buffer_size);
	if (unlikely(!buffer)) {
		uk_pr_err("malloc failed\n");
		start = end = 0;
		goto out;
	}
	memset(buffer, '1', buffer_size);
	buffer[buffer_size - 1] = '\0';

	/* Write for the first time */

	printf("Running for the first time:\n");
	start = _clock();

	for (BYTES i = 0; i < num_intervals; i++) {
		off = intervals[interval_order[i]].off;
		len = intervals[interval_order[i]].len;

		BYTES iterations = len / buffer_size;
		BYTES rest = len % buffer_size;

		for (BYTES j = 0; j < iterations; j++) {
			memcpy(buffer, (char *)vfdev->dax_addr + off
				+ buffer_size * j,
				buffer_size);
		}
		if (rest > 0) {
			memcpy(buffer, (char *)vfdev->dax_addr + off
				+ buffer_size * iterations,
				rest);
		}
	}

	end = _clock();

	__nanosec result_ms = nanosec_to_milisec(end - start);
	printf("Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);
	printf("Running for the second time:\n");

	/* Write for the second time */

	start = _clock();

	for (BYTES i = 0; i < num_intervals; i++) {
		off = intervals[interval_order[i]].off;
		len = intervals[interval_order[i]].len;

		BYTES iterations = len / buffer_size;
		BYTES rest = len % buffer_size;

		for (BYTES j = 0; j < iterations; j++) {
			memcpy(buffer, (char *)vfdev->dax_addr + off
				+ buffer_size * j,
				buffer_size);
		}
		if (rest > 0) {
			memcpy(buffer, (char *)vfdev->dax_addr + off
				+ buffer_size * iterations,
				rest);
		}
	}

	end = _clock();

	rc = uk_fuse_request_removemapping_legacy(fusedev, file.nodeid,
		file.fh, 0, size);
	if (unlikely(rc)) {
		uk_pr_err("uk_fuse_request_removemapping_legacy has failed \n");
		start = end = 0;
		goto out1;
	}

	rc = uk_fuse_request_release(fusedev, false,
		file.nodeid, file.fh);
	if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			start = end = 0;
			goto out1;
	}
	rc = uk_fuse_request_forget(fusedev, file.nodeid, 1);
	if (rc) {
			uk_pr_err("uk_fuse_request_forget has failed \n");
			start = end = 0;
			goto out1;
	}

out1:
	free(buffer);
out:
	return end - start;
}


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