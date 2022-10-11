#include "uk/scenario_runners.h"
#include "fcntl.h"

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
	fuse_file_context dc = {.is_dir = true, .name = "create_files_DAX",
		.mode = 0777, .flags = O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK,
		.parent_nodeid = 1
	};
	fuse_file_context results_fc = {.is_dir = false, .name = "results.csv"};
	fuse_file_context measurement_fcs[arr_size];
	uint32_t bytes_transferred;
	uint64_t results_offset = 0;

	char measurement_text[100] = {0};

	// create a separate directory for this experiment
	rc = uk_fuse_mkdir_request(fusedev, 1, dc.name,
		dc.mode, &dc.nodeid, &dc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_mkdir_request has failed \n");
		return;
	}
	rc = uk_fuse_create_request(fusedev, results_fc.parent_nodeid,
		results_fc.name, results_fc.flags, results_fc.mode,
		&results_fc.nodeid, &results_fc.fh, &results_fc.nlookup);
	if (rc) {
		uk_pr_err("uk_fuse_mkdir_request has failed \n");
		return;
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
			return;
		}

		FILES amount = amount_arr[i];
		printf("###########################\n");
		printf("%lu/%lu. Measuring creating %lu files\n",
			i+1, arr_size, amount);

		__nanosec result;
		__nanosec result_ms;
		__nanosec total = 0;

		// measures 'measurements' times, how long the creation 'amount' of files takes
		for (int i = 0; i < measurements; i++) {
				printf("    Measurement %d/%d running...\n", i + 1, measurements);

			result = create_files(fusedev, amount);

			sprintf(measurement_text, "%lu\n", result);
			rc = uk_fuse_write_request(fusedev,
				measurement_fcs[i].nodeid, measurement_fcs[i].fh,
				measurement_text, strlen(measurement_text),
				0, &bytes_transferred);
			if (rc) {
				uk_pr_err("uk_fuse_write_request has failed \n");
				return;
			}
			result_ms = nanosec_to_milisec(result);
			printf("    Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);

			total += result;
		}

		total /= measurements;
		sprintf(measurement_text, "%lu,%lu\n", amount, total);
		rc = uk_fuse_write_request(fusedev, results_fc.nodeid,
			results_fc.fh, measurement_text,
			sizeof(measurement_text), results_offset,
			&bytes_transferred);
		if (rc) {
			uk_pr_err("uk_fuse_write_request has failed \n");
			return;
		}
		results_offset += bytes_transferred;

		__nanosec total_ms = nanosec_to_milisec(total);

		printf("%d measurements successfully conducted\n", measurements);
		printf("Creating %lu files took on average: %lums %.3fs \n", amount, total_ms, (double) total_ms / 1000);

		rc = uk_fuse_release_request(fusedev, false,
			measurement_fcs[i].nodeid, measurement_fcs[i].fh);
		if (rc) {
			uk_pr_err("uk_fuse_release_request has failed \n");
			return;
		}
	}

	rc = uk_fuse_release_request(fusedev, false,
		results_fc.nodeid, results_fc.fh);
	if (rc) {
			uk_pr_err("uk_fuse_release_request has failed \n");
			return;
	}
	rc = uk_fuse_release_request(fusedev, true,
		dc.nodeid, dc.fh);
	if (rc) {
		uk_pr_err("uk_fuse_release_request has failed \n");
		return;
	}
}

// void remove_files_runner(FILES *amount_arr, size_t arr_size, int measurements) {

//     // create a separate directory for this experiment

//     #ifdef __Unikraft__
//     char dir_name[] = "remove_files_unikraft";
//     #elif __linux__
//     char dir_name[] = "remove_files_linux";
//     #endif
//     mkdir(dir_name, 0777);
//     if (chdir(dir_name) == -1) {
//         printf("Error: could not change directory to %s\n", dir_name);
//     }

//     char fname_results[] = "results.csv";
//     FILE *fp_results = fopen(fname_results, "w");

//     for (size_t i = 0; i < arr_size; i++) { // conducts measurement for each amount of files
//         char fname[17+DIGITS(i)];
//         sprintf(fname, "measurement_%lu.csv", i);
//         FILE *fp_measurement = fopen(fname, "w");

//         FILES amount = amount_arr[i];

//         // measures 'measurements' times the creation of 'amount' files takes

//         printf("###########################\n");
//         printf("%lu/%lu. Measuring removing %lu files\n", i+1, arr_size, amount);

//         __nanosec result;
//         __nanosec result_ms;
//         __nanosec total = 0;

//         for (int i = 0; i < measurements; i++) {
//             printf("Measurement %d/%d running...\n", i + 1, measurements);

//             result = remove_files(amount);

//             fprintf(fp_measurement, "%lu\n", result);
//             result_ms = nanosec_to_milisec(result);
//             printf("Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);

//             total += result;
//         }

//         total /= measurements;
//         fprintf(fp_results, "%lu,%lu\n", amount, total);
//         __nanosec total_ms = nanosec_to_milisec(total);

//         printf("%d measurements successfully conducted\n", measurements);
//         printf("Removing %lu files took on average: %lums %.3fs \n", amount, total_ms, (double) total_ms / 1000);

//         fclose(fp_measurement);
//     }

//     fclose(fp_results);
//     chdir("..");
// }

// void list_dir_runner(FILES *amount_arr, size_t arr_size, int measurements) {

//     // create outer directory

//     #ifdef __Unikraft__
//     char dir_name[] = "list_dir_unikraft";
//     #elif __linux__
//     char dir_name[] = "list_dir_linux";
//     #endif
//     mkdir(dir_name, 0777);
//     if (chdir(dir_name) == -1) {
//         printf("Error: could not change directory to %s\n", dir_name);
//     }

//     // file for mean

//     char fname_results[] = "results.csv";
//     FILE *fp_results = fopen(fname_results, "w");


//     for (size_t i = 0; i < arr_size; i++) { // conducts measurement for each amount of files

//         // directory for files to create and list in

//         char list_dir_name[] = "list_dir";
//         mkdir(list_dir_name, 0777);
//         chdir(list_dir_name);

//         chdir("..");
//         char fname[17+DIGITS(i)];
//         sprintf(fname, "measurement_%lu.csv", i);
//         FILE *fp_measurement = fopen(fname, "w");
//         chdir(list_dir_name);

//         FILES file_amount = amount_arr[i];

//         // initializing dummy files

//         int max_file_name_length = 7 + DIGITS(file_amount - 1);
//         char *file_names = (char*) malloc(file_amount*max_file_name_length); // implicit 2D array
//         init_filenames(file_amount, max_file_name_length, file_names);

//         for (FILES i = 0; i < file_amount; i++) {
//             create_file_of_size(file_names + i*max_file_name_length, 0);
//         }

//         printf("###########################\n");
//         printf("%lu/%lu. Measuring listing %lu files\n", i+1, arr_size, file_amount);

//         __nanosec result;
//         __nanosec result_ms;
//         __nanosec total = 0;

//         // measuring 'measurements' times the listing of 'file_amount' files takes
//         for (int i = 0; i < measurements; i++) {
//             #ifdef __linux__
//             system("sync; echo 3 > /proc/sys/vm/drop_caches");
//             #endif

//             printf("Measurement %d/%d running...\n", i + 1, measurements);

//             result = list_dir(file_amount);

//             fprintf(fp_measurement, "%lu\n", result);
//             result_ms = nanosec_to_milisec(result);
//             printf("Result: %lums %.3fs\n", result_ms, (double) result_ms / 1000);

//             total += result;
//         }

//         total /= measurements;
//         fprintf(fp_results, "%lu,%lu\n", file_amount, total);
//         __nanosec total_ms = nanosec_to_milisec(total);

//         printf("%d measurements successfully conducted\n", measurements);
//         printf("Listing %lu files took on average: %lums %.3fs \n", file_amount, total_ms, (double) total_ms / 1000);

//         // deleting all created files and directories

//         for (FILES i = 0; i < file_amount; i++) {
//             if (remove(file_names + i*max_file_name_length) != 0) {
//                 fprintf(stderr, "Failed to remove \"%s\" file\n", file_names + i*max_file_name_length);
//             }
//         }
//         chdir("..");
//         int ret = rmdir(list_dir_name);
//         if (ret == -1) {
//             printf("Failed to remove directory %s\n", dir_name);
//         }

//         fclose(fp_measurement);
//     }

//     fclose(fp_results);
//     chdir("..");
// }

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