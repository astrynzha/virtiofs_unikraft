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
	rc = uk_fuse_mkdir_request(fusedev, 1, dc.name,
		dc.mode, &dc.nodeid, &dc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_mkdir_request has failed \n");
		goto free;
	}
	rc = uk_fuse_create_request(fusedev, dc.nodeid,
		results_fc.name, results_fc.flags, results_fc.mode,
		&results_fc.nodeid, &results_fc.fh, &results_fc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_create_request has failed \n");
		goto free;
	}

	for (size_t i = 0; i < arr_size; i++) { // conducts measurement for each given amount of files
		sprintf(measurement_fcs[i].name, "measurement_%lu.csv", i);
		rc = uk_fuse_create_request(fusedev, dc.nodeid,
			measurement_fcs[i].name,
			O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK, 0777,
			&measurement_fcs[i].nodeid, &measurement_fcs[i].fh,
			&measurement_fcs[i].nlookup);
		if (rc) {
			uk_pr_err("uk_fuse_create_request has failed \n");
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
			rc = uk_fuse_write_request(fusedev,
				measurement_fcs[i].nodeid, measurement_fcs[i].fh,
				measurement_text, strlen(measurement_text),
				meas_file_offset, &bytes_transferred);
			if (rc) {
				uk_pr_err("uk_fuse_write_request has failed \n");
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
		rc = uk_fuse_write_request(fusedev, results_fc.nodeid,
			results_fc.fh, measurement_text,
			strlen(measurement_text), results_offset,
			&bytes_transferred);
		if (rc) {
			uk_pr_err("uk_fuse_write_request has failed \n");
			goto free;
		}
		results_offset += bytes_transferred;
		measurement_text[0] = '\0';

		__nanosec total_ms = nanosec_to_milisec(total);

		printf("%d measurements successfully conducted\n", measurements);
		printf("Creating %lu files took on average: %lums %.3fs \n", amount, total_ms, (double) total_ms / 1000);

		rc = uk_fuse_release_request(fusedev, false,
			measurement_fcs[i].nodeid, measurement_fcs[i].fh);
		if (rc) {
			uk_pr_err("uk_fuse_release_request has failed \n");
			goto free;
		}
	}

	rc = uk_fuse_release_request(fusedev, false,
		results_fc.nodeid, results_fc.fh);
	if (rc) {
			uk_pr_err("uk_fuse_release_request has failed \n");
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
	rc = uk_fuse_mkdir_request(fusedev, 1, dc.name,
		dc.mode, &dc.nodeid, &dc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_mkdir_request has failed \n");
		goto free;
	}
	rc = uk_fuse_create_request(fusedev, dc.nodeid,
		results_fc.name, results_fc.flags, results_fc.mode,
		&results_fc.nodeid, &results_fc.fh, &results_fc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_create_request has failed \n");
		goto free;
	}

	for (size_t i = 0; i < arr_size; i++) { // conducts measurement for each amount of files
		sprintf(measurement_fcs[i].name, "measurement_%lu.csv", i);
		rc = uk_fuse_create_request(fusedev, dc.nodeid,
			measurement_fcs[i].name,
			O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK, 0777,
			&measurement_fcs[i].nodeid, &measurement_fcs[i].fh,
			&measurement_fcs[i].nlookup);
		if (rc) {
			uk_pr_err("uk_fuse_create_request has failed \n");
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
			rc = uk_fuse_write_request(fusedev,
				measurement_fcs[i].nodeid, measurement_fcs[i].fh,
				measurement_text,
				strlen(measurement_text),
				meas_file_offset, &bytes_transferred);
			if (rc) {
				uk_pr_err("uk_fuse_write_request has failed \n");
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
		rc = uk_fuse_write_request(fusedev, results_fc.nodeid,
			results_fc.fh, measurement_text,
			strlen(measurement_text),
			results_offset, &bytes_transferred);
		if (rc) {
			uk_pr_err("uk_fuse_write_request has failed \n");
			goto free;
		}
		results_offset += bytes_transferred;
		measurement_text[0] = '\0';

		__nanosec total_ms = nanosec_to_milisec(total);

		printf("%d measurements successfully conducted\n", measurements);
		printf("Removing %lu files took on average: %lums %.3fs \n", amount, total_ms, (double) total_ms / 1000);

		rc = uk_fuse_release_request(fusedev, false,
			measurement_fcs[i].nodeid, measurement_fcs[i].fh);
		if (rc) {
			uk_pr_err("uk_fuse_release_request has failed \n");
			goto free;
		}
	}

	rc = uk_fuse_release_request(fusedev, false,
		results_fc.nodeid, results_fc.fh);
	if (rc) {
		uk_pr_err("uk_fuse_release_request has failed \n");
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
	fuse_file_context *dummy_fcs;
	uint64_t meas_file_offset = 0;
	uint32_t bytes_transferred = 0;
	uint64_t results_offset = 0;

	char measurement_text[100] = {0};
	measurement_fcs = calloc(arr_size, sizeof(fuse_file_context));
	if (!measurement_fcs) {
		uk_pr_err("calloc failed");
		return;
	}
	// create outer directory
	rc = uk_fuse_mkdir_request(fusedev, 1, dc.name,
		dc.mode, &dc.nodeid, &dc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_mkdir_request has failed \n");
		goto free;
	}
	rc = uk_fuse_create_request(fusedev, dc.nodeid,
		results_fc.name, results_fc.flags, results_fc.mode,
		&results_fc.nodeid, &results_fc.fh, &results_fc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_create_request has failed \n");
		goto free;
	}

	for (size_t i = 0; i < arr_size; i++) { // conducts measurement for each amount of files
		rc = uk_fuse_mkdir_request(fusedev, dc.nodeid,
			dc_ls.name, dc_ls.mode, &dc_ls.nodeid,
			&dc_ls.nlookup);
		if (rc) {
			uk_pr_err("uk_fuse_mkdir_request has failed \n");
			goto free;
		}
		sprintf(measurement_fcs[i].name, "measurement_%lu.csv", i);
		rc = uk_fuse_create_request(fusedev, dc.nodeid,
			measurement_fcs[i].name,
			O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK, 0777,
			&measurement_fcs[i].nodeid, &measurement_fcs[i].fh,
			&measurement_fcs[i].nlookup);
		if (rc) {
			uk_pr_err("uk_fuse_create_request has failed \n");
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
			rc = uk_fuse_create_request(fusedev, dc_ls.nodeid,
				file_names + i*max_file_name_length,
				O_WRONLY | O_CREAT | O_EXCL, 0777,
				&dummy_fcs[i].nodeid, &dummy_fcs[i].fh,
				&dummy_fcs[i].nlookup);
			if (rc) {
				uk_pr_err("uk_fuse_create_request has failed \n");
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
			rc = uk_fuse_write_request(fusedev,
				measurement_fcs[i].nodeid,
				measurement_fcs[i].fh, measurement_text,
				strlen(measurement_text), meas_file_offset,
				&bytes_transferred);
			if (rc) {
				uk_pr_err("uk_fuse_write_request has failed \n");
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
		rc = uk_fuse_write_request(fusedev, results_fc.nodeid,
			results_fc.fh, measurement_text,
			strlen(measurement_text),
			results_offset, &bytes_transferred);
		if (rc) {
			uk_pr_err("uk_fuse_write_request has failed \n");
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
			rc = uk_fuse_unlink_request(fusedev,
				file_names + i*max_file_name_length,
				false, dummy_fcs[i].nodeid,
				dummy_fcs[i].nlookup,
				dc_ls.nodeid);
			if (rc) {
				uk_pr_err("uk_fuse_unlink_request has failed \n");
				free(dummy_fcs);
				goto free;
			}
		}
		rc = uk_fuse_unlink_request(fusedev,
				dc_ls.name,
				true, dc_ls.nodeid,
				dc_ls.nlookup,
				dc.nodeid);
		if (rc) {
			uk_pr_err("uk_fuse_unlink_request has failed \n");
			free(dummy_fcs);
			goto free;
		}

		rc = uk_fuse_release_request(fusedev, false,
			measurement_fcs[i].nodeid, measurement_fcs[i].fh);
		if (rc) {
			uk_pr_err("uk_fuse_release_request has failed \n");
			free(dummy_fcs);
			goto free;
		}

		free(dummy_fcs);
	}


	rc = uk_fuse_release_request(fusedev, false,
		results_fc.nodeid, results_fc.fh);
	if (rc) {
			uk_pr_err("uk_fuse_release_request has failed \n");
			goto free;
	}

free:
	free(measurement_fcs);
}

// void write_seq_runner(BYTES bytes, BYTES *buffer_size_arr, size_t arr_size, int measurements) {

//     // create a separate directory for this experiment

//     #ifdef __Unikraft__
//     char dir_name[] = "write_seq_unikraft";
//     #elif __linux__
//     char dir_name[] = "write_seq_linux";
//     #endif
//     mkdir(dir_name, 0777);
//     if (chdir(dir_name) == -1) {
//         printf("Error: could not change directory to %s\n", dir_name);
//     }

//     char fname_results[] = "results.csv";
//     FILE *fp_results = fopen(fname_results, "w");

//     for (size_t i = 0; i < arr_size; i++) { // conducts measurement for each buffer_size
//         char fname[17+DIGITS(i)];
//         sprintf(fname, "measurement_%lu.csv", i);
//         FILE *fp_measurement = fopen(fname, "w");

//         BYTES buffer_size = buffer_size_arr[i];

//         printf("###########################\n");
//         printf("%lu/%lu. Measuring sequential write of %llu megabytes with a buffer of %lluB\n",
//             i+1, arr_size, B_TO_MB(bytes), buffer_size);

//         __nanosec result;
//         __nanosec result_ms;
//         __nanosec total = 0;

//         for (int i = 0; i < measurements; i++) {
//             printf("Measurement %d/%d running...\n", i + 1, measurements);

//             result = write_seq(bytes, buffer_size);

//             fprintf(fp_measurement, "%lu\n", result);
//             result_ms = nanosec_to_milisec(result);
//             printf("Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);

//             total += result;
//         }

//         total /= measurements;
//         fprintf(fp_results, "%llu,%lu\n", buffer_size, total);
//         __nanosec total_ms = nanosec_to_milisec(total);

//         printf("%d measurements successfully conducted\n", measurements);
//         printf("Average result: %lums %.3fs \n", total_ms, (double) total_ms / 1000);

//         fclose(fp_measurement);
//     }

//     fclose(fp_results);
//     chdir("..");
// }

// void write_randomly_runner(const char *filename, BYTES bytes, BYTES *buffer_size_arr,
//     size_t arr_size, BYTES lower_write_limit, BYTES upper_write_limit, int measurements) {
// 	FILE *file;
// 	file = fopen(filename, "r+");
// 	if (file == NULL) {
// 		fprintf(stderr, "Error opening file '%s'.\n", filename);
// 		exit(EXIT_FAILURE);
// 	}

//     // create a separate directory for this experiment

//     #ifdef __Unikraft__
//     char dir_name[] = "write_rand_unikraft";
//     #elif __linux__
//     char dir_name[] = "write_rand_linux";
//     #endif
//     mkdir(dir_name, 0777);
//     if (chdir(dir_name) == -1) {
//         printf("Error: could not change directory to %s\n", dir_name);
//     }

//     char fname_results[] = "results.csv";
//     FILE *fp_results = fopen(fname_results, "w");

//     for (size_t i = 0; i < arr_size; i++) { // conducts measurement for each buffer_size
//         char fname[17+DIGITS(i)];
//         sprintf(fname, "measurement_%lu.csv", i);
//         FILE *fp_measurement = fopen(fname, "w");

//         BYTES buffer_size = buffer_size_arr[i];

//         printf("###########################\n");
//         printf("%lu/%lu. Measuring random write of %llu megabytes with a buffer of %lluB\n",
//             i+1, arr_size, B_TO_MB(bytes), buffer_size);

//         __nanosec result;
//         __nanosec result_ms;
//         __nanosec total = 0;

//         for (int i = 0; i < measurements; i++) {
//             printf("Measurement %d/%d running...\n", i + 1, measurements);

//             srand(time(NULL)); // setting random seed
//             result = write_randomly(file, bytes, buffer_size, lower_write_limit, upper_write_limit);

//             fprintf(fp_measurement, "%lu\n", result);
//             result_ms = nanosec_to_milisec(result);
//             printf("Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);

//             total += result;
//         }

//         total /= measurements;
//         fprintf(fp_results, "%llu,%lu\n", buffer_size, total);
//         __nanosec total_ms = nanosec_to_milisec(total);

//         printf("%d measurements successfully conducted\n", measurements);
//         printf("Writing %llu non-sequentially took on average: %lums %.3fs \n",
//             B_TO_MB(bytes), total_ms, (double) total_ms / 1000);

//         fclose(fp_measurement);
//     }

//     fclose(fp_results);
// 	fclose(file);
//     chdir("..");
// }

// /*
//     'bytes' has to be less than file size.
// */
// void read_seq_runner(const char *filename, BYTES bytes,
//     BYTES *buffer_size_arr, size_t arr_size, int measurements) {

//     FILE *file = fopen(filename, "r");
//     if (file == NULL) {
//         fprintf(stderr, "Error opening file '%s'.\n", filename);
//         exit(EXIT_FAILURE);
//     }


//     // create a separate directory for this experiment

//     #ifdef __Unikraft__
//     char dir_name[] = "read_seq_unikraft";
//     #elif __linux__
//     char dir_name[] = "read_seq_linux";
//     #endif
//     mkdir(dir_name, 0777);
//     if (chdir(dir_name) == -1) {
//         printf("Error: could not change directory to %s\n", dir_name);
//     }

//     char fname_results[] = "results.csv";
//     FILE *fp_results = fopen(fname_results, "w");

//     for (size_t i = 0; i < arr_size; i++) { // conducts measurement for each buffer_size
//         char fname[17+DIGITS(i)];
//         sprintf(fname, "measurement_%lu.csv", i);
//         FILE *fp_measurement = fopen(fname, "w");

//         BYTES buffer_size = buffer_size_arr[i];

//         printf("###########################\n");
//         printf("%lu/%lu. Measuring sequential read of %llu megabytes with a buffer of %lluB\n",
//             i+1, arr_size, B_TO_MB(bytes), buffer_size);

//         __nanosec result;
//         __nanosec result_ms;
//         __nanosec total = 0;

//         for (int i = 0; i < measurements; i++) {
//             #ifdef __linux__
//             system("sync; echo 3 > /proc/sys/vm/drop_caches");
//             #endif
//             printf("Measurement %d/%d running...\n", i + 1, measurements);

//             result = read_seq(file, bytes, buffer_size);
//             rewind(file);

//             fprintf(fp_measurement, "%lu\n", result);
//             result_ms = nanosec_to_milisec(result);
//             printf("Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);

//             total += result;
//         }

//         total /= measurements;
//         fprintf(fp_results, "%llu,%lu\n", buffer_size, total);
//         __nanosec total_ms = nanosec_to_milisec(total);

//         printf("%d measurements successfully conducted\n", measurements);
//         printf("Reading %lluMB with %lluB buffer took on average: %lums %.3fs \n", B_TO_MB(bytes), buffer_size, total_ms, (double) total_ms / 1000);

//         fclose(fp_measurement);
//     }

//     fclose(fp_results);
//     fclose(file);
//     chdir("..");
// }

// void read_randomly_runner(const char *filename, BYTES bytes, BYTES *buffer_size_arr,
//     size_t arr_size, BYTES lower_read_limit, BYTES upper_read_limit, int measurements) {
// 	FILE *file;
// 	file = fopen(filename, "r");
// 	if (file == NULL) {
// 		fprintf(stderr, "Error opening file '%s'.\n", filename);
// 		exit(EXIT_FAILURE);
// 	}

//     // create a separate directory for this experiment

//     #ifdef __Unikraft__
//     char dir_name[] = "read_rand_unikraft";
//     #elif __linux__
//     char dir_name[] = "read_rand_linux";
//     #endif
//     mkdir(dir_name, 0777);
//     if (chdir(dir_name) == -1) {
//         printf("Error: could not change directory to %s\n", dir_name);
//     }

//     char fname_results[] = "results.csv";
//     FILE *fp_results = fopen(fname_results, "w");

//     for (size_t i = 0; i < arr_size; i++) { // conducts measurement for each buffer_size
//         char fname[17+DIGITS(i)];
//         sprintf(fname, "measurement_%lu.csv", i);
//         FILE *fp_measurement = fopen(fname, "w");

//         BYTES buffer_size = buffer_size_arr[i];

//         printf("###########################\n");
//         printf("%lu/%lu. Measuring random read of %llu megabytes with a buffer of %lluB\n",
//             i+1, arr_size, B_TO_MB(bytes), buffer_size);

//         __nanosec result;
//         __nanosec result_ms;
//         __nanosec total = 0;

//         for (int i = 0; i < measurements; i++) {
//             #ifdef __linux__
//             system("sync; echo 3 > /proc/sys/vm/drop_caches");
//             #endif
//             printf("Measurement %d/%d running...\n", i + 1, measurements);

//             srand(time(NULL)); // setting random seed
//             result = read_randomly(file, bytes, buffer_size, lower_read_limit, upper_read_limit);

//             fprintf(fp_measurement, "%lu\n", result);
//             result_ms = nanosec_to_milisec(result);
//             printf("Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);

//             total += result;
//         }

//         total /= measurements;
//         fprintf(fp_results, "%llu,%lu\n", buffer_size, total);
//         __nanosec total_ms = nanosec_to_milisec(total);

//         printf("%d measurements successfully conducted\n", measurements);
//         printf("Reading %lluMB with a buffer of %lluB took on average: %lums %.3fs \n",
//             B_TO_MB(bytes), buffer_size, total_ms, (double) total_ms / 1000);

//         fclose(fp_measurement);
//     }

//     fclose(fp_results);
// 	fclose(file);
//     chdir("..");
// }

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