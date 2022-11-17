#include "uk/scenario_runners.h"
#include "fcntl.h"
#include "stdbool.h"
#include "uk/print.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef DEBUGMODE
#include <errno.h>
#include <string.h>
#endif // DEBUGMODE


#include "uk/helper_functions.h"
#include "uk/measurement_scenarios.h"
#include "uk/time_functions.h"

/* ukfuse */
#include "uk/fusedev_core.h"
#include "uk/fuse.h"


void create_files_runner(struct uk_fuse_dev *fusedev, FILES *amount_arr,
			 size_t arr_size, int measurements) {
	int rc = 0;
	fuse_file_context dc = {.is_dir = true, .name = "create_files_virtiofs",
		.mode = 0777, .flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK,
		.parent_nodeid = 1
	};
	fuse_file_context results_fc = {.is_dir = false, .name = "results.csv",
		.mode = 0777, .flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK
	};
	fuse_file_context *measurement_fcs;
	uint64_t meas_file_offset = 0;
	uint32_t bytes_transferred = 0;
	uint64_t results_offset = 0;

	char measurement_text[100] = {0};
	measurement_fcs = calloc(arr_size, sizeof(fuse_file_context));
	if (!measurement_fcs) {
		uk_pr_err("calloc failed \n");
		return;
	}

	// create a separate directory for this experiment
	rc = uk_fuse_request_mkdir(fusedev, 1, dc.name,
		dc.mode, &dc.nodeid, &dc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_request_mkdir has failed \n");
		goto free;
	}
	rc = uk_fuse_request_create(fusedev, dc.nodeid,
		results_fc.name, results_fc.flags, results_fc.mode,
		&results_fc.nodeid, &results_fc.fh, &results_fc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_request_create has failed \n");
		goto free;
	}

	for (size_t i = 0; i < arr_size; i++) { // conducts measurement for each given amount of files
		sprintf(measurement_fcs[i].name, "measurement_%lu.csv", i);
		rc = uk_fuse_request_create(fusedev, dc.nodeid,
			measurement_fcs[i].name,
			O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK, 0777,
			&measurement_fcs[i].nodeid, &measurement_fcs[i].fh,
			&measurement_fcs[i].nlookup);
		if (rc) {
			uk_pr_err("uk_fuse_request_create has failed \n");
			goto free;
		}

		FILES amount = amount_arr[i];
		printf("###########################\n");
		printf("%lu/%lu. Measuring creating %lu files\n",
			i+1, arr_size, amount);

		__nanosec result;
		__nanosec result_ms;
		__nanosec total = 0;

		// measures 'measurements' times, how long the creation 'amount' of files takes
		for (int j = 0; j < measurements; j++) {
			printf("    Measurement %d/%d running...\n", j + 1, measurements);

			result = create_files(fusedev, amount);

			sprintf(measurement_text, "%lu\n", result);
			rc = uk_fuse_request_write(fusedev,
				measurement_fcs[i].nodeid, measurement_fcs[i].fh,
				measurement_text, strlen(measurement_text),
				meas_file_offset, &bytes_transferred);
			if (rc) {
				uk_pr_err("uk_fuse_request_write has failed \n");
				goto free;
			}
			meas_file_offset += bytes_transferred;
			measurement_text[0] = '\0';

			result_ms = nanosec_to_milisec(result);
			printf("    Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);

			total += result;
		}
		meas_file_offset = 0;

		total /= measurements;
		sprintf(measurement_text, "%lu,%lu\n", amount, total);
		rc = uk_fuse_request_write(fusedev, results_fc.nodeid,
			results_fc.fh, measurement_text,
			strlen(measurement_text), results_offset,
			&bytes_transferred);
		if (rc) {
			uk_pr_err("uk_fuse_request_write has failed \n");
			goto free;
		}
		results_offset += bytes_transferred;
		measurement_text[0] = '\0';

		__nanosec total_ms = nanosec_to_milisec(total);

		printf("%d measurements successfully conducted\n", measurements);
		printf("Creating %lu files took on average: %lums %.3fs \n", amount, total_ms, (double) total_ms / 1000);

		rc = uk_fuse_request_release(fusedev, false,
			measurement_fcs[i].nodeid, measurement_fcs[i].fh);
		if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			goto free;
		}
	}

	rc = uk_fuse_request_release(fusedev, false,
		results_fc.nodeid, results_fc.fh);
	if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			goto free;
	}

free:
	free(measurement_fcs);
}

void remove_files_runner(struct uk_fuse_dev *fusedev, FILES *amount_arr,
			 size_t arr_size, int measurements)
{
	int rc = 0;
	fuse_file_context dc = {.is_dir = true, .name = "remove_files_virtiofs",
		.mode = 0777, .flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK,
		.parent_nodeid = 1
	};
	fuse_file_context results_fc = {.is_dir = false, .name = "results.csv",
		.mode = 0777, .flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK
	};
	fuse_file_context *measurement_fcs;
	uint64_t meas_file_offset = 0;
	uint32_t bytes_transferred = 0;
	uint64_t results_offset = 0;

	char measurement_text[100] = {0};
	measurement_fcs = calloc(arr_size, sizeof(fuse_file_context));
	if (!measurement_fcs) {
		uk_pr_err("calloc failed \n");
		return;
	}

	// create a separate directory for this experiment
	rc = uk_fuse_request_mkdir(fusedev, 1, dc.name,
		dc.mode, &dc.nodeid, &dc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_request_mkdir has failed \n");
		goto free;
	}
	rc = uk_fuse_request_create(fusedev, dc.nodeid,
		results_fc.name, results_fc.flags, results_fc.mode,
		&results_fc.nodeid, &results_fc.fh, &results_fc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_request_create has failed \n");
		goto free;
	}

	for (size_t i = 0; i < arr_size; i++) { // conducts measurement for each amount of files
		sprintf(measurement_fcs[i].name, "measurement_%lu.csv", i);
		rc = uk_fuse_request_create(fusedev, dc.nodeid,
			measurement_fcs[i].name,
			O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK, 0777,
			&measurement_fcs[i].nodeid, &measurement_fcs[i].fh,
			&measurement_fcs[i].nlookup);
		if (rc) {
			uk_pr_err("uk_fuse_request_create has failed \n");
			goto free;
		}

		FILES amount = amount_arr[i];

		// measures 'measurements' times the creation of 'amount' files takes

		printf("###########################\n");
		printf("%lu/%lu. Measuring removing %lu files\n", i+1, arr_size, amount);

		__nanosec result;
		__nanosec result_ms;
		__nanosec total = 0;

		for (int j = 0; j < measurements; j++) {
			printf("Measurement %d/%d running...\n", j + 1,
				measurements);

			result = remove_files(fusedev, amount);

			sprintf(measurement_text, "%lu\n", result);
			rc = uk_fuse_request_write(fusedev,
				measurement_fcs[i].nodeid, measurement_fcs[i].fh,
				measurement_text,
				strlen(measurement_text),
				meas_file_offset, &bytes_transferred);
			if (rc) {
				uk_pr_err("uk_fuse_request_write has failed \n");
				goto free;
			}
			meas_file_offset += bytes_transferred;
			measurement_text[0] = '\0';

			result_ms = nanosec_to_milisec(result);
			printf("Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);

			total += result;
		}
		meas_file_offset = 0;

		total /= measurements;
		sprintf(measurement_text, "%lu,%lu\n", amount, total);
		rc = uk_fuse_request_write(fusedev, results_fc.nodeid,
			results_fc.fh, measurement_text,
			strlen(measurement_text),
			results_offset, &bytes_transferred);
		if (rc) {
			uk_pr_err("uk_fuse_request_write has failed \n");
			goto free;
		}
		results_offset += bytes_transferred;
		measurement_text[0] = '\0';

		__nanosec total_ms = nanosec_to_milisec(total);

		printf("%d measurements successfully conducted\n", measurements);
		printf("Removing %lu files took on average: %lums %.3fs \n", amount, total_ms, (double) total_ms / 1000);

		rc = uk_fuse_request_release(fusedev, false,
			measurement_fcs[i].nodeid, measurement_fcs[i].fh);
		if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			goto free;
		}
	}

	rc = uk_fuse_request_release(fusedev, false,
		results_fc.nodeid, results_fc.fh);
	if (rc) {
		uk_pr_err("uk_fuse_request_release has failed \n");
		goto free;
	}

free:
	free(measurement_fcs);
}

void list_dir_runner(struct uk_fuse_dev *fusedev, FILES *amount_arr,
		     size_t arr_size, int measurements)
{
	int rc = 0;
	fuse_file_context dc = {.is_dir = true, .name = "list_dir_virtiofs",
		.mode = 0777, .flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK,
		.parent_nodeid = 1
	};
	fuse_file_context dc_ls = {.is_dir = true, .name = "sample_files",
		.mode = 0777, .flags = O_WRONLY | O_CREAT | O_EXEC | O_NONBLOCK,
	};
	fuse_file_context results_fc = {.is_dir = false, .name = "results.csv",
		.mode = 0777, .flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK
	};
	fuse_file_context *measurement_fcs;
	char measurement_text[100] = {0};
	fuse_file_context *dummy_fcs;
	uint64_t meas_file_offset = 0;
	uint32_t bytes_transferred = 0;
	uint64_t results_offset = 0;

	measurement_fcs = calloc(arr_size, sizeof(fuse_file_context));
	if (!measurement_fcs) {
		uk_pr_err("calloc failed");
		return;
	}
	// create outer directory
	rc = uk_fuse_request_mkdir(fusedev, 1, dc.name,
		dc.mode, &dc.nodeid, &dc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_request_mkdir has failed \n");
		goto free;
	}
	rc = uk_fuse_request_create(fusedev, dc.nodeid,
		results_fc.name, results_fc.flags, results_fc.mode,
		&results_fc.nodeid, &results_fc.fh, &results_fc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_request_create has failed \n");
		goto free;
	}

	for (size_t i = 0; i < arr_size; i++) { // conducts measurement for each amount of files
		rc = uk_fuse_request_mkdir(fusedev, dc.nodeid,
			dc_ls.name, dc_ls.mode, &dc_ls.nodeid,
			&dc_ls.nlookup);
		if (rc) {
			uk_pr_err("uk_fuse_request_mkdir has failed \n");
			goto free;
		}
		sprintf(measurement_fcs[i].name, "measurement_%lu.csv", i);
		rc = uk_fuse_request_create(fusedev, dc.nodeid,
			measurement_fcs[i].name,
			O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK, 0777,
			&measurement_fcs[i].nodeid, &measurement_fcs[i].fh,
			&measurement_fcs[i].nlookup);
		if (rc) {
			uk_pr_err("uk_fuse_request_create has failed \n");
			goto free;
		}

		FILES amount = amount_arr[i];

		// initializing dummy files

		int max_file_name_length = 7 + DIGITS(amount - 1);
		char *file_names = (char*) malloc(amount*max_file_name_length); // implicit 2D array
		init_filenames(amount, max_file_name_length, file_names);

		dummy_fcs = calloc(amount, sizeof(fuse_file_context));
		if (!dummy_fcs) {
			uk_pr_err("calloc failed \n");
			goto free;
		}
		for (FILES i = 0; i < amount; i++) {
			rc = uk_fuse_request_create(fusedev, dc_ls.nodeid,
				file_names + i*max_file_name_length,
				O_WRONLY | O_CREAT | O_EXCL, 0777,
				&dummy_fcs[i].nodeid, &dummy_fcs[i].fh,
				&dummy_fcs[i].nlookup);
			if (rc) {
				uk_pr_err("uk_fuse_request_create has failed \n");
				free(dummy_fcs);
				goto free;
			}
		}

		printf("###########################\n");
		printf("%lu/%lu. Measuring listing %lu files\n", i+1, arr_size, amount);

		__nanosec result;
		__nanosec result_ms;
		__nanosec total = 0;

		// measuring 'measurements' times the listing of 'file_amount' files takes
		for (int j = 0; j < measurements; j++) {
			#ifdef __linux__
			system("sync; echo 3 > /proc/sys/vm/drop_caches");
			#endif

			printf("Measurement %d/%d running...\n", j + 1, measurements);

			result = list_dir(fusedev, amount, dc_ls.nodeid);

			sprintf(measurement_text, "%lu\n", result);
			rc = uk_fuse_request_write(fusedev,
				measurement_fcs[i].nodeid,
				measurement_fcs[i].fh, measurement_text,
				strlen(measurement_text), meas_file_offset,
				&bytes_transferred);
			if (rc) {
				uk_pr_err("uk_fuse_request_write has failed \n");
				free(dummy_fcs);
				goto free;
			}
			meas_file_offset += bytes_transferred;
			measurement_text[0] = '\0';

			result_ms = nanosec_to_milisec(result);
			printf("Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);

			total += result;
		}
		meas_file_offset = 0;

		total /= measurements;
		sprintf(measurement_text, "%lu,%lu\n", amount, total);
		rc = uk_fuse_request_write(fusedev, results_fc.nodeid,
			results_fc.fh, measurement_text,
			strlen(measurement_text),
			results_offset, &bytes_transferred);
		if (rc) {
			uk_pr_err("uk_fuse_request_write has failed \n");
			free(dummy_fcs);
			goto free;
		}
		results_offset += bytes_transferred;
		measurement_text[0] = '\0';

		__nanosec total_ms = nanosec_to_milisec(total);

		printf("%d measurements successfully conducted\n", measurements);
		printf("Listing %lu files took on average: %lums %.3fs \n", amount, total_ms, (double) total_ms / 1000);

		// deleting all created files and directories

		for (FILES i = 0; i < amount; i++) {
			rc = uk_fuse_request_unlink(fusedev,
				file_names + i*max_file_name_length,
				false, dummy_fcs[i].nodeid,
				dummy_fcs[i].nlookup,
				dc_ls.nodeid);
			if (rc) {
				uk_pr_err("uk_fuse_request_unlink has failed \n");
				free(dummy_fcs);
				goto free;
			}
		}
		rc = uk_fuse_request_unlink(fusedev,
				dc_ls.name,
				true, dc_ls.nodeid,
				dc_ls.nlookup,
				dc.nodeid);
		if (rc) {
			uk_pr_err("uk_fuse_request_unlink has failed \n");
			free(dummy_fcs);
			goto free;
		}

		rc = uk_fuse_request_release(fusedev, false,
			measurement_fcs[i].nodeid, measurement_fcs[i].fh);
		if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			free(dummy_fcs);
			goto free;
		}

		free(dummy_fcs);
	}

	rc = uk_fuse_request_release(fusedev, false,
		results_fc.nodeid, results_fc.fh);
	if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			goto free;
	}

free:
	free(measurement_fcs);
}

/**
 * @brief
 *
 * If with_dax==true, a file called "1G_file" of size 100GiB has to exist
 * in the root directory of the shared file system.
 *
 *
 * @param fusedev
 * @param vfdev
 * @param with_dax
 * @param bytes
 * @param buffer_size_arr
 * @param arr_size
 * @param measurements
 */
void write_seq_runner(struct uk_fuse_dev *fusedev, struct uk_vfdev *vfdev,
	bool with_dax, BYTES bytes, BYTES *buffer_size_arr, size_t arr_size,
	int measurements)
{
	int rc = 0;
	fuse_file_context dc = {.is_dir = true, .mode = 0777,
		.flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK,
		.parent_nodeid = 1
	};
	fuse_file_context results_fc = {.is_dir = false, .name = "results.csv",
		.mode = 0777, .flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK
	};
	fuse_file_context *measurement_fcs;
	char measurement_text[100] = {0};
	uint64_t meas_file_offset = 0;
	uint32_t bytes_transferred = 0;
	uint64_t results_offset = 0;


	strcpy(dc.name, with_dax ? "write_seq_DAX" : "write_seq_FUSE");
	measurement_fcs = calloc(arr_size, sizeof(fuse_file_context));
	if (!measurement_fcs) {
		uk_pr_err("calloc failed");
		return;
	}
	// create a separate directory for this experiment
	rc = uk_fuse_request_mkdir(fusedev, 1, dc.name,
		dc.mode, &dc.nodeid, &dc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_request_mkdir has failed \n");
		goto free;
	}

	rc = uk_fuse_request_create(fusedev, dc.nodeid,
		results_fc.name, results_fc.flags,
		results_fc.mode, &results_fc.nodeid, &results_fc.fh,
		&results_fc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_request_create has failed \n");
		goto free;
	}

	for (size_t i = 0; i < arr_size; i++) { // conducts measurement for each buffer_size
		sprintf(measurement_fcs[i].name, "measurement_%lu.csv", i);
		rc = uk_fuse_request_create(fusedev, dc.nodeid,
			measurement_fcs[i].name,
			O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK, 0777,
			&measurement_fcs[i].nodeid, &measurement_fcs[i].fh,
			&measurement_fcs[i].nlookup);
		if (rc) {
			uk_pr_err("uk_fuse_request_create has failed \n");
			goto free;
		}

		BYTES buffer_size = buffer_size_arr[i];

		printf("###########################\n");
		printf("%lu/%lu. Measuring sequential write of %llu megabytes with a buffer of %lluB\n",
		i+1, arr_size, B_TO_MB(bytes), buffer_size);

		__nanosec result;
		__nanosec result_ms;
		__nanosec total = 0;

		for (int j = 0; j < measurements; j++) {
			printf("Measurement %d/%d running...\n", j + 1, measurements);

			if (with_dax) {
				result = write_seq_dax(fusedev, vfdev, bytes,
					buffer_size);
			} else {
				result = write_seq_fuse(fusedev, bytes, buffer_size,
					dc.nodeid);
			}

			sprintf(measurement_text, "%lu\n", result);
			rc = uk_fuse_request_write(fusedev, measurement_fcs[i].nodeid,
				measurement_fcs[i].fh, measurement_text,
				strlen(measurement_text),
				meas_file_offset, &bytes_transferred);
			if (rc) {
				uk_pr_err("uk_fuse_request_write has failed \n");
				goto free;
			}
			meas_file_offset += bytes_transferred;
			measurement_text[0] = '\0';

			result_ms = nanosec_to_milisec(result);
			printf("Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);

			total += result;
		}
		meas_file_offset = 0;

		total /= measurements;
		sprintf(measurement_text, "%llu,%lu\n", buffer_size, total);
		rc = uk_fuse_request_write(fusedev, results_fc.nodeid,
			results_fc.fh, measurement_text,
			strlen(measurement_text),
			results_offset, &bytes_transferred);
		if (rc) {
			uk_pr_err("uk_fuse_request_write has failed \n");
			goto free;
		}
		results_offset += bytes_transferred;
		measurement_text[0] = '\0';

		__nanosec total_ms = nanosec_to_milisec(total);

		printf("%d measurements successfully conducted\n", measurements);
		printf("Average result: %lums %.3fs \n", total_ms, (double) total_ms / 1000);

		rc = uk_fuse_request_release(fusedev, false,
			measurement_fcs[i].nodeid, measurement_fcs[i].fh);
		if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			goto free;
		}
	}

	rc = uk_fuse_request_release(fusedev, false,
		results_fc.nodeid, results_fc.fh);
	if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			goto free;
	}

free:
	free(measurement_fcs);
}


/**
 * @brief
 *
 * If with_dax==true, a file called "1G_file" of size 100GiB has to exist
 * in the root directory of the shared file system.
 *
 * @param fusedev
 * @param vfdev
 * @param with_dax
 * @param bytes
 * @param buffer_size_arr
 * @param arr_size
 * @param measurements
 */
void read_seq_runner(struct uk_fuse_dev *fusedev, struct uk_vfdev *vfdev,
	bool with_dax, BYTES bytes, BYTES *buffer_size_arr, size_t arr_size,
	int measurements)
{
	int rc = 0;
	fuse_file_context dc = {.is_dir = true, .mode = 0777,
		.flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK,
		.parent_nodeid = 1
	};
	fuse_file_context results_fc = {.is_dir = false, .name = "results.csv",
		.mode = 0777, .flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK
	};
	fuse_file_context measurement_fc;
	char measurement_text[100] = {0};
	uint64_t meas_file_offset = 0;
	uint32_t bytes_transferred = 0;
	uint64_t results_offset = 0;

	strcpy(dc.name, with_dax ? "read_seq_DAX" : "read_seq_FUSE");
	rc = uk_fuse_request_mkdir(fusedev, 1, dc.name,
		dc.mode, &dc.nodeid, &dc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_request_mkdir has failed \n");
		return;
	}

	rc = uk_fuse_request_create(fusedev, dc.nodeid,
		results_fc.name, results_fc.flags,
		results_fc.mode, &results_fc.nodeid, &results_fc.fh,
		&results_fc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_request_create has failed \n");
		return;
	}

	for (size_t i = 0; i < arr_size; i++) { // conducts measurement for each buffer_size
		sprintf(measurement_fc.name, "measurement_%lu.csv", i);
		rc = uk_fuse_request_create(fusedev, dc.nodeid,
			measurement_fc.name,
			O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK, 0777,
			&measurement_fc.nodeid, &measurement_fc.fh,
			&measurement_fc.nlookup);
		if (rc) {
			uk_pr_err("uk_fuse_request_create has failed \n");
			return;
		}

		BYTES buffer_size = buffer_size_arr[i];

		printf("###########################\n");
		printf("%lu/%lu. Measuring sequential read of %llu megabytes with a buffer of %lluB\n",
		i+1, arr_size, B_TO_MB(bytes), buffer_size);

		__nanosec result;
		__nanosec result_ms;
		__nanosec total = 0;

		for (int j = 0; j < measurements; j++) {
			printf("Measurement %d/%d running...\n", j + 1, measurements);

			if (with_dax) {
				result = read_seq_dax(fusedev, vfdev, bytes,
					buffer_size);
			} else {
				result = read_seq_fuse(fusedev, bytes, buffer_size);
			}

			sprintf(measurement_text, "%lu\n", result);
			rc = uk_fuse_request_write(fusedev,
				measurement_fc.nodeid, measurement_fc.fh,
				measurement_text, strlen(measurement_text),
				meas_file_offset, &bytes_transferred);
			if (rc) {
				uk_pr_err("uk_fuse_request_write has failed \n");
				return;
			}
			meas_file_offset += bytes_transferred;
			measurement_text[0] = '\0';

			result_ms = nanosec_to_milisec(result);
			printf("Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);

			total += result;
		}
		meas_file_offset = 0;

		total /= measurements;
		sprintf(measurement_text, "%llu,%lu\n", buffer_size, total);
		rc = uk_fuse_request_write(fusedev, results_fc.nodeid,
			results_fc.fh, measurement_text,
			strlen(measurement_text),
			results_offset, &bytes_transferred);
		if (rc) {
			uk_pr_err("uk_fuse_request_write has failed \n");
			return;
		}
		results_offset += bytes_transferred;
		measurement_text[0] = '\0';


		__nanosec total_ms = nanosec_to_milisec(total);

		printf("%d measurements successfully conducted\n", measurements);
		printf("Reading %lluMB with %lluB buffer took on average: %lums %.3fs \n", B_TO_MB(bytes), buffer_size, total_ms, (double) total_ms / 1000);

		rc = uk_fuse_request_release(fusedev, false,
			measurement_fc.nodeid, measurement_fc.fh);
		if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			return;
		}
	}

	rc = uk_fuse_request_release(fusedev, false,
		results_fc.nodeid, results_fc.fh);
	if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			return;
	}
}


void write_randomly_runner(struct uk_fuse_dev *fusedev, struct uk_vfdev *vfdev,
	bool with_dax, BYTES bytes, BYTES *buffer_size_arr, size_t arr_size,
	BYTES lower_write_limit, BYTES upper_write_limit, int measurements)
{
	int rc = 0;
	fuse_file_context dc = {.is_dir = true, .mode = 0777,
		.flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK,
		.parent_nodeid = 1
	};
	fuse_file_context results_fc = { .is_dir = false, .name = "results.csv",
		.mode = 0777, .flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK
	};
	fuse_file_context measurements_fc = {.is_dir = false,
		.mode = 0777, .flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK
	};
	char measurement_text[100] = {0};
	uint64_t meas_file_offset = 0;
	uint32_t bytes_transferred = 0;
	uint64_t results_offset = 0;


	// create a separate directory for this experiment
	strcpy(dc.name, with_dax ? "write_rand_DAX" : "write_rand_FUSE");
	rc = uk_fuse_request_mkdir(fusedev, 1, dc.name,
		dc.mode, &dc.nodeid, &dc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_request_mkdir has failed \n");
		return;
	}

	rc = uk_fuse_request_create(fusedev, dc.nodeid,
		results_fc.name, results_fc.flags,
		results_fc.mode, &results_fc.nodeid, &results_fc.fh,
		&results_fc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_request_create has failed \n");
		return;
	}

	for (size_t i = 0; i < arr_size; i++) { // conducts measurement for each buffer_size
		sprintf(measurements_fc.name, "measurement_%lu.csv", i);
		rc = uk_fuse_request_create(fusedev, dc.nodeid,
			measurements_fc.name,
			O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK, 0777,
			&measurements_fc.nodeid, &measurements_fc.fh,
			&measurements_fc.nlookup);
		if (rc) {
			uk_pr_err("uk_fuse_request_create has failed \n");
			return;
		}

		BYTES buffer_size = buffer_size_arr[i];

		printf("###########################\n");
		printf("%lu/%lu. Measuring random write of %llu megabytes with a buffer of %lluB\n",
		i+1, arr_size, B_TO_MB(bytes), buffer_size);

		__nanosec result;
		__nanosec result_ms;
		__nanosec total = 0;

		for (int i = 0; i < measurements; i++) {
			printf("Measurement %d/%d running...\n", i + 1, measurements);

			/* TODOFS: write_randomly_dax & write_randomly_fuse */
			if (with_dax) {
				result = write_randomly_dax(fusedev, vfdev,
					bytes, buffer_size,
					lower_write_limit, upper_write_limit);

			} else {
				result = write_randomly_fuse(fusedev, bytes,
					buffer_size, lower_write_limit,
					upper_write_limit);
			}

			sprintf(measurement_text, "%lu\n", result);
			rc = uk_fuse_request_write(fusedev, measurements_fc.nodeid,
				measurements_fc.fh, measurement_text,
				strlen(measurement_text),
				meas_file_offset, &bytes_transferred);
			if (rc) {
				uk_pr_err("uk_fuse_request_write has failed \n");
				return;
			}
			meas_file_offset += bytes_transferred;
			measurement_text[0] = '\0';

			result_ms = nanosec_to_milisec(result);
			printf("Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);

			total += result;
		}
		meas_file_offset = 0;

		total /= measurements;
		sprintf(measurement_text, "%llu,%lu\n", buffer_size, total);
		rc = uk_fuse_request_write(fusedev, results_fc.nodeid,
			results_fc.fh, measurement_text,
			strlen(measurement_text),
			results_offset, &bytes_transferred);
		if (rc) {
			uk_pr_err("uk_fuse_request_write has failed \n");
			return;
		}
		results_offset += bytes_transferred;
		measurement_text[0] = '\0';

		__nanosec total_ms = nanosec_to_milisec(total);

		printf("%d measurements successfully conducted\n", measurements);
		printf("Writing %llu non-sequentially took on average: %lums %.3fs \n",
		B_TO_MB(bytes), total_ms, (double) total_ms / 1000);

		rc = uk_fuse_request_release(fusedev, false,
			measurements_fc.nodeid, measurements_fc.fh);
		if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			return;
		}
	}

	rc = uk_fuse_request_release(fusedev, false,
		results_fc.nodeid, results_fc.fh);
	if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			return;
	}
}

void read_randomly_runner(struct uk_fuse_dev *fusedev, struct uk_vfdev *vfdev,
	bool with_dax, BYTES bytes, BYTES *buffer_size_arr, size_t arr_size,
	BYTES lower_read_limit, BYTES upper_read_limit, int measurements)
{
	int rc = 0;
	fuse_file_context dc = {.is_dir = true, .mode = 0777,
		.flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK,
		.parent_nodeid = 1
	};
	fuse_file_context results_fc = { .is_dir = false, .name = "results.csv",
		.mode = 0777, .flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK
	};
	fuse_file_context measurements_fc = {.is_dir = false,
		.mode = 0777, .flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK
	};
	char measurement_text[100] = {0};
	uint64_t meas_file_offset = 0;
	uint32_t bytes_transferred = 0;
	uint64_t results_offset = 0;

	// create a separate directory for this experiment
	strcpy(dc.name, with_dax ? "read_rand_DAX" : "read_rand_FUSE");
	rc = uk_fuse_request_mkdir(fusedev, 1, dc.name,
		dc.mode, &dc.nodeid, &dc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_request_mkdir has failed \n");
		return;
	}

	rc = uk_fuse_request_create(fusedev, dc.nodeid,
		results_fc.name, results_fc.flags,
		results_fc.mode, &results_fc.nodeid, &results_fc.fh,
		&results_fc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_request_create has failed \n");
		return;
	}

	for (size_t i = 0; i < arr_size; i++) { // conducts measurement for each buffer_size
		sprintf(measurements_fc.name, "measurement_%lu.csv", i);
		rc = uk_fuse_request_create(fusedev, dc.nodeid,
			measurements_fc.name,
			O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK, 0777,
			&measurements_fc.nodeid, &measurements_fc.fh,
			&measurements_fc.nlookup);
		if (rc) {
			uk_pr_err("uk_fuse_request_create has failed \n");
			return;
		}

		BYTES buffer_size = buffer_size_arr[i];

		printf("###########################\n");
		printf("%lu/%lu. Measuring random read of %llu megabytes with a buffer of %lluB\n",
		i+1, arr_size, B_TO_MB(bytes), buffer_size);

		__nanosec result;
		__nanosec result_ms;
		__nanosec total = 0;

		for (int i = 0; i < measurements; i++) {
			printf("Measurement %d/%d running...\n", i + 1, measurements);

			result = with_dax ?
				read_randomly_dax(fusedev, vfdev,
					bytes, buffer_size, lower_read_limit,
					upper_read_limit)
					  :
				read_randomly_fuse(fusedev, bytes,
					buffer_size, lower_read_limit,
					upper_read_limit);

			sprintf(measurement_text, "%lu\n", result);
			rc = uk_fuse_request_write(fusedev, measurements_fc.nodeid,
				measurements_fc.fh, measurement_text,
				strlen(measurement_text),
				meas_file_offset, &bytes_transferred);
			if (rc) {
				uk_pr_err("uk_fuse_request_write has failed \n");
				return;
			}
			meas_file_offset += bytes_transferred;
			measurement_text[0] = '\0';

			result_ms = nanosec_to_milisec(result);
			printf("Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);

			total += result;
		}
		meas_file_offset = 0;

		total /= measurements;
		sprintf(measurement_text, "%llu,%lu\n", buffer_size, total);
		rc = uk_fuse_request_write(fusedev, results_fc.nodeid,
			results_fc.fh, measurement_text,
			strlen(measurement_text),
			results_offset, &bytes_transferred);
		if (rc) {
			uk_pr_err("uk_fuse_request_write has failed \n");
			return;
		}
		results_offset += bytes_transferred;
		measurement_text[0] = '\0';

		__nanosec total_ms = nanosec_to_milisec(total);

		printf("%d measurements successfully conducted\n", measurements);
		printf("Reading %lluMB with a buffer of %lluB took on average: %lums %.3fs \n",
		B_TO_MB(bytes), buffer_size, total_ms, (double) total_ms / 1000);

		rc = uk_fuse_request_release(fusedev, false,
			measurements_fc.nodeid, measurements_fc.fh);
		if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			return;
		}
	}
	rc = uk_fuse_request_release(fusedev, false,
		results_fc.nodeid, results_fc.fh);
	if (rc) {
			uk_pr_err("uk_fuse_request_release has failed \n");
			return;
	}
}

// /*
//     Creates a file, filled with 'X' charachters, of size `bytes`, with a given filename
// */
// FILE *create_file_of_size(const char *filename, BYTES bytes) {
// 	FILE *file;
// 	BYTES buffer_size = KB(1);
// 	char buffer[buffer_size];

// 	for (BYTES i = 0; i < buffer_size; i++) {
// 		buffer[i] = 'X';
// 	}

// 	file = fopen(filename, "w");
// 	if (file == NULL) {
// 		fprintf(stderr, "Error opening file '%s'.\n", filename);
// 		exit(EXIT_FAILURE);
// 	}

// 	for (BYTES i = 0; i < B_TO_KB(bytes); i++) {
// 		fwrite(buffer, sizeof(char), buffer_size, file);
// 	}
//     BYTES rest = bytes % buffer_size;
//     if (rest > 0) {
// 		if (rest != fwrite(buffer, sizeof(char), rest, file)) {
// 			fprintf(stderr, "Failed to write the rest of the file\n");
// 		}
//     }

// 	fclose(file);

//     // TODO: surround with debug guards
// 	// printf("File %s of size %lluB created\n", filename, bytes);

// 	return file;
// }