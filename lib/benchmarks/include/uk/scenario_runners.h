#ifndef SCENARIO_RUNNERS_H
#define SCENARIO_RUNNERS_H

#include "helper_functions.h"

#include <stdio.h>

/* ukfuse */
#include "uk/fusedev_core.h"

FILE *create_file_of_size(const char *filename, BYTES bytes);

void create_files_runner(struct uk_fuse_dev *fusedev, FILES *amount_arr,
			 size_t arr_size, int measurements);
void remove_files_runner(FILES *amount_arr, size_t arr_size, int measurements);
void list_dir_runner(FILES *amount_arr, size_t arr_size, int measurements);

void write_seq_runner(BYTES bytes, BYTES *buffer_size_arr, size_t arr_size,
    int measurements);
void write_randomly_runner(const char *filename, BYTES bytes, BYTES *buffer_size_arr,
    size_t arr_size, BYTES lower_write_limit, BYTES upper_write_limit, int measurements);

void read_seq_runner(const char *filename, BYTES bytes,
    BYTES *buffer_size_arr, size_t arr_size, int measurements);
void read_randomly_runner(const char *filename, BYTES bytes, BYTES *buffer_size_arr,
    size_t arr_size, BYTES lower_read_limit, BYTES upper_read_limit, int measurements);

#endif