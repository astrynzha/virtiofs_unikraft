#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include "uk/bench_tests.h"
#include "uk/measurement_scenarios.h"
#include "uk/helper_functions.h"
#include "uk/print.h"
#include "uk/scenario_runners.h"

/* ukfuse */
#include "uk/fusedev.h"
#include "uk/fusedev_trans.h"
#include "uk/fusedev_core.h"
#include "uk/fuse.h"
/* virtiofs */
#include "uk/vfdev.h"
#include "uk/vf_bench_tests.h"

/* returns @p base to the power of @p exp */
BYTES bpow(BYTES base, BYTES exp)
{
	BYTES result = 1;
	for (;;)
	{
		if (exp & 1)
			result *= base;
		exp >>= 1;
		if (!exp)
			break;
		base *= base;
	}

	return result;
}

void bench_test(void)
{
	int rc = 0;
	struct uk_fusedev_trans *trans;
	struct uk_fuse_dev *dev;
	struct uk_vfdev vfdev;

	#ifdef __linux__
	char platform[6] = "Linux";
    chdir("guest_fs");
	#elif __Unikraft__
	char platform[9] = "Unikraft";
	#endif
	DEBUG_PRINT("__________________________\n");
	DEBUG_PRINT("Running in DEBUGMODE\n");
	DEBUG_PRINT("Running on %s\n", platform);
	DEBUG_PRINT("__________________________\n");


	trans = uk_fusedev_trans_get_default();
	dev = uk_fusedev_connect(trans, "myfs", NULL);

	if ((rc = uk_fuse_request_init(dev))) {
		uk_pr_err("uk_fuse_init has failed \n");
		return;
	}

	vfdev = uk_vf_connect();

	int max_files = 17;
	int min_files = 1;
	int arr_size_files = max_files - min_files + 1;

	FILES amount[arr_size_files];

	printf("amount\n");
	for (int i = min_files; i < max_files + 1; i++) { // buffer_sizes = {16, 32, 64, ..., 2^max_pow}
		amount[i-min_files] = bpow(2, i);
		printf("%lu\n", amount[i-min_files]);
	}

	int measurements_files = 6;

	// create_files_runner(dev, amount, 17, measurements_files);
	// remove_files_runner(dev, amount, 17, measurements_files);
	list_dir_runner(dev, amount, arr_size_files, measurements_files);


/*
	int max_pow2 = 20;
	int min_pow2 = 4;
	int arr_size = max_pow2 - min_pow2 + 1;

	BYTES bytes_arr_FUSE[arr_size];
	BYTES buffer_size_arr[arr_size];
	BYTES interval_len_arr[arr_size];

	BYTES bytes_arr_DAX[arr_size];

	printf("buffer sizes\n");
	for (int i = min_pow2; i < max_pow2 + 1; i++) { // buffer_sizes = {16, 32, 64, ..., 2^max_pow}
		buffer_size_arr[i-min_pow2] = bpow(2, i);
		bytes_arr_FUSE[i-min_pow2] = buffer_size_arr[i-min_pow2] * bpow(2, 13);
		bytes_arr_DAX[i-min_pow2] = bpow(2, 20 + 13);
		interval_len_arr[i-min_pow2] = buffer_size_arr[i-min_pow2];
		printf("buffer_size: %llu\n\
			bytes: %llu\n\
			interval_len: %llu\n\
			---------------------\n",
			buffer_size_arr[i - min_pow2],
			bytes_arr_FUSE[i-min_pow2],
			interval_len_arr[i-min_pow2]);
	}
	int measurements = 3;

	// write_seq_runner(dev, &vfdev, DAX_SECOND_RUN, bytes_arr_DAX,
	// 		 buffer_size_arr, arr_size, measurements);
	// write_seq_runner(dev, &vfdev, DAX_FIRST_RUN, bytes_arr_DAX,
	// 		 buffer_size_arr, arr_size, measurements);
	write_seq_runner(dev, &vfdev, NO_DAX, bytes_arr_FUSE,
			 buffer_size_arr, arr_size, measurements);

	// read_seq_runner(dev, &vfdev, DAX_SECOND_RUN, bytes_arr_DAX,
	// 		buffer_size_arr, arr_size, measurements);
	// read_seq_runner(dev, &vfdev, DAX_FIRST_RUN, bytes_arr_DAX,
	// 		buffer_size_arr, arr_size, measurements);
	read_seq_runner(dev, &vfdev, NO_DAX, bytes_arr_FUSE,
			buffer_size_arr, arr_size, measurements);

	// write_randomly_runner(dev, &vfdev, DAX_SECOND_RUN, bytes_arr_DAX,
	// 	buffer_size_arr, interval_len_arr, arr_size, measurements);
	// write_randomly_runner(dev, &vfdev, DAX_FIRST_RUN, bytes_arr_DAX,
	// 	buffer_size_arr, interval_len_arr, arr_size, measurements);
	write_randomly_runner(dev, &vfdev, NO_DAX, bytes_arr_FUSE,
		buffer_size_arr, interval_len_arr,
		arr_size, measurements);

	// read_randomly_runner(dev, &vfdev, DAX_SECOND_RUN, bytes_arr_DAX,
	// 	buffer_size_arr, interval_len_arr,
	// 	arr_size, measurements);
	// read_randomly_runner(dev, &vfdev, DAX_FIRST_RUN, bytes_arr_DAX,
	// 	buffer_size_arr, interval_len_arr,
	// 	arr_size, measurements);
	read_randomly_runner(dev, &vfdev, NO_DAX, bytes_arr_FUSE,
		buffer_size_arr, interval_len_arr,
		arr_size, measurements);

*/
}
