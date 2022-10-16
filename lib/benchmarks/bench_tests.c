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

	// create_file_of_size("data_100M", MB(100));

	// int max_pow = 17;
	// FILES amount[max_pow];
	// printf("amount\n");
	// for (int i = 0; i < max_pow; i++) { // buffer_sizes = {16, 32, 64, ..., 2^max_pow}
	// 	amount[i] = bpow(2, i + 1);
	// 	printf("%lu\n", amount[i]);
	// }


	// create_files_runner(dev, amount, 17, 10);
	// remove_files_runner(dev, amount, 17, 10);
	// list_dir_runner(dev, amount, 17, 10);

	int max_pow2 = 27;
	BYTES buffer_sizes[max_pow2-3];
	printf("buffer sizes\n");
	for (int i = 0; i < max_pow2-3; i++) { // buffer_sizes = {16, 32, 64, ..., 2^max_pow}
		buffer_sizes[i] = bpow(2, i+4);
		printf("%llu\n", buffer_sizes[i]);
	}


	// write_seq_runner(dev, &vfdev, false, MB(100), buffer_sizes, max_pow2-3, 10);
	write_seq_runner(dev, &vfdev, true, MB(100), buffer_sizes, max_pow2-3, 10);
	// write_randomly_runner("data_100M", MB(100), buffer_sizes, max_pow2-3, MB(0.01), MB(0.1), 10);

	// read_seq_runner(dev, &vfdev, true, MB(100),
	// 	buffer_sizes, max_pow2-3, 10);
	// write_randomly_runner(dev, &vfdev, false, MB(100),
	// 	buffer_sizes, max_pow2-3,
	// 	MB(0.01), MB(0.1), 10);
	// read_randomly_runner("data_100M", MB(100), buffer_sizes, max_pow2-3, MB(0.01), MB(0.1), 10);
}
