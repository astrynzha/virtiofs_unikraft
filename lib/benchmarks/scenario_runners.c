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

			result = remove_files(fusedev, amount, j + 1);

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
	fuse_file_context results_fc = {.is_dir = false, .name = "results.csv",
		.mode = 0777, .flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK
	};
	fuse_file_context *measurement_fcs;
	char measurement_text[100] = {0};
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
		printf("%lu/%lu. Measuring listing %lu files\n", i+1, arr_size, amount);

		__nanosec result;
		__nanosec result_ms;
		__nanosec total = 0;

		// measuring 'measurements' times the listing of 'file_amount' files takes
		for (int j = 0; j < measurements; j++) {

			printf("Measurement %d/%d running...\n", j + 1, measurements);


			result = list_dir(fusedev, amount, j + 1);

			sprintf(measurement_text, "%lu\n", result);
			rc = uk_fuse_request_write(fusedev,
				measurement_fcs[i].nodeid,
				measurement_fcs[i].fh, measurement_text,
				strlen(measurement_text), meas_file_offset,
				&bytes_transferred);
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
		printf("Listing %lu files took on average: %lums %.3fs \n", amount, total_ms, (double) total_ms / 1000);

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
	rc = uk_fuse_request_forget(fusedev, dc.nodeid, 0);

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
	enum dax dax, BYTES *bytes_arr, BYTES *buffer_size_arr, size_t arr_size,
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


	char dax_mode[256];
	switch (dax) {
		case NO_DAX:
			strcpy(dc.name, "write_seq_FUSE");
			strcpy(dax_mode, "no");
			break;
		case DAX_FIRST_RUN:
			strcpy(dc.name, "write_seq_DAX");
			strcpy(dax_mode, "first run");
			break;
		case DAX_SECOND_RUN:
			strcpy(dc.name, "write_seq_DAX_2");
			strcpy(dax_mode, "second run");
			break;
		default:
			printf("unkown DAX mode\n");
			#ifdef __Unikraft__
			UK_CRASH("Crashing...\n");
			#elif __linux__
			exit(0);
			#endif
	}
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
		BYTES bytes = bytes_arr[i];

		printf("###########################\n");
		printf("%lu/%lu. Sequential write.\n\
		DAX: %s,\n\
		Megaytes: %llu,\n\
		Buffer_size: %lluB\n",
		i+1, arr_size, dax_mode,
		B_TO_MB(bytes), buffer_size);

		__nanosec result;
		__nanosec result_ms;
		__nanosec total = 0;

		for (int j = 0; j < measurements; j++) {
			printf("Measurement %d/%d running...\n", j + 1, measurements);

			switch (dax) {
				case NO_DAX:
					result = write_seq_fuse(fusedev, bytes, buffer_size);
					break;
				case DAX_FIRST_RUN:
					result = write_seq_dax(fusedev, vfdev, bytes,
					buffer_size);
					break;
				case DAX_SECOND_RUN:
					result = write_seq_dax_2(fusedev, vfdev, bytes,
					buffer_size);
					break;
				default:
					printf("unkown DAX mode\n");
					#ifdef __Unikraft__
					UK_CRASH("Crashing...\n");
					#elif __linux__
					exit(0);
					#endif
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
	enum dax dax, BYTES *bytes_arr, BYTES *buffer_size_arr, size_t arr_size,
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

	char dax_mode[256];
	switch (dax) {
		case NO_DAX:
			strcpy(dc.name, "read_seq_FUSE");
			strcpy(dax_mode, "no");
			break;
		case DAX_FIRST_RUN:
			strcpy(dc.name, "read_seq_DAX");
			strcpy(dax_mode, "first run");
			break;
		case DAX_SECOND_RUN:
			strcpy(dc.name, "read_seq_DAX_2");
			strcpy(dax_mode, "second run");
			break;
		default:
			printf("unkown DAX mode\n");
			#ifdef __Unikraft__
			UK_CRASH("Crashing...\n");
			#elif __linux__
			exit(0);
			#endif
	}
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
		BYTES bytes = bytes_arr[i];

		printf("###########################\n");
		printf("%lu/%lu. Sequential read.\n\
		DAX: %s,\n\
		Megaytes: %llu,\n\
		Buffer_size: %lluB\n",
		i+1, arr_size, dax_mode,
		B_TO_MB(bytes), buffer_size);

		__nanosec result;
		__nanosec result_ms;
		__nanosec total = 0;

		for (int j = 0; j < measurements; j++) {
			printf("Measurement %d/%d running...\n", j + 1, measurements);

			switch (dax) {
				case NO_DAX:
					result = read_seq_fuse(fusedev, bytes, buffer_size);
					break;
				case DAX_FIRST_RUN:
					result = read_seq_dax(fusedev, vfdev, bytes,
					buffer_size);
					break;
				case DAX_SECOND_RUN:
					result = read_seq_dax_2(fusedev, vfdev, bytes,
					buffer_size);
					break;
				default:
					printf("unkown DAX mode\n");
					#ifdef __Unikraft__
					UK_CRASH("Crashing...\n");
					#elif __linux__
					exit(0);
					#endif
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
	enum dax dax, BYTES *bytes_arr, BYTES *buffer_size_arr,
	BYTES *interval_len_arr, size_t arr_size, int measurements)
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


	char dax_mode[256];
	switch (dax) {
		case NO_DAX:
			strcpy(dc.name, "write_rand_FUSE");
			strcpy(dax_mode, "no");
			break;
		case DAX_FIRST_RUN:
			strcpy(dc.name, "write_rand_DAX");
			strcpy(dax_mode, "first run");
			break;
		case DAX_SECOND_RUN:
			strcpy(dc.name, "write_rand_DAX_2");
			strcpy(dax_mode, "second run");
			break;
		default:
			printf("unkown DAX mode\n");
			#ifdef __Unikraft__
			UK_CRASH("Crashing...\n");
			#elif __linux__
			exit(0);
			#endif
	}
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
		BYTES bytes = bytes_arr[i];
		BYTES interval_len = interval_len_arr[i];

		printf("###########################\n");
		printf("%lu/%lu. Random write.\n\
		DAX: %s,\n\
		Megaytes: %llu,\n\
		Buffer_size: %lluB\n\
		Interval_length: %llu\n",
		i+1, arr_size, dax_mode,
		B_TO_MB(bytes), buffer_size, interval_len);

		__nanosec result;
		__nanosec result_ms;
		__nanosec total = 0;

		for (int i = 0; i < measurements; i++) {
			printf("Measurement %d/%d running...\n", i + 1, measurements);

			switch (dax) {
				case NO_DAX:
					result = write_randomly_fuse(fusedev, bytes,
					buffer_size, interval_len);
					break;
				case DAX_FIRST_RUN:
					result = write_randomly_dax(fusedev, vfdev,
					bytes, buffer_size,
					interval_len);
					break;
				case DAX_SECOND_RUN:
					result = write_randomly_dax_2(fusedev, vfdev,
					bytes, buffer_size,
					interval_len);
					break;
				default:
					printf("unkown DAX mode\n");
					#ifdef __Unikraft__
					UK_CRASH("Crashing...\n");
					#elif __linux__
					exit(0);
					#endif
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
	enum dax dax, BYTES *bytes_arr, BYTES *buffer_size_arr,
	BYTES *interval_len_arr, size_t arr_size, int measurements)
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

	char dax_mode[256];
	switch (dax) {
		case NO_DAX:
			strcpy(dc.name, "read_rand_FUSE");
			strcpy(dax_mode, "no");
			break;
		case DAX_FIRST_RUN:
			strcpy(dc.name, "read_rand_DAX");
			strcpy(dax_mode, "first run");
			break;
		case DAX_SECOND_RUN:
			strcpy(dc.name, "read_rand_DAX_2");
			strcpy(dax_mode, "second run");
			break;
		default:
			printf("unkown DAX mode\n");
			#ifdef __Unikraft__
			UK_CRASH("Crashing...\n");
			#elif __linux__
			exit(0);
			#endif
	}
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
		BYTES bytes = bytes_arr[i];
		BYTES interval_len = interval_len_arr[i];

		printf("###########################\n");
		printf("%lu/%lu. Random read.\n\
		DAX: %s,\n\
		Megaytes: %llu,\n\
		Buffer_size: %lluB\n\
		Interval_length: %llu\n",
		i+1, arr_size, dax_mode,
		B_TO_MB(bytes), buffer_size, interval_len);

		__nanosec result;
		__nanosec result_ms;
		__nanosec total = 0;

		for (int i = 0; i < measurements; i++) {
			printf("Measurement %d/%d running...\n", i + 1, measurements);

			switch (dax) {
				case NO_DAX:
					result = read_randomly_fuse(fusedev, bytes,
					buffer_size, interval_len);
					break;
				case DAX_FIRST_RUN:
					result = read_randomly_dax(fusedev, vfdev,
					bytes, buffer_size, interval_len);
					break;
				case DAX_SECOND_RUN:
					result = read_randomly_dax_2(fusedev, vfdev,
					bytes, buffer_size, interval_len);
					break;
				default:
					printf("unkown DAX mode\n");
					#ifdef __Unikraft__
					UK_CRASH("Crashing...\n");
					#elif __linux__
					exit(0);
					#endif
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