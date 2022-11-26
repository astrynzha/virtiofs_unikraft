#ifndef HELPER_FUNCTIONS_H
#define HELPER_FUNCTIONS_H

#include <stdio.h>
#include "uk/bench_math.h"
#include "uk/vfdev.h"

// #define NDEBUG
#include <assert.h>

#define DEBUGMODE 0
#ifdef DEBUGMODE
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

// how many digits a number has. Cannot be less than 1.
#define DIGITS(num) ((int)((log10_custom(MAX(num, 1u))+1)*sizeof(char)))

typedef unsigned long FILES;
typedef unsigned long long BYTES;

void init_filenames(FILES amount, int max_filename_length, char *file_names);


#define KB(n) ((BYTES) (n)*1024)
#define MB(n) ((BYTES) ((n) * KB(1024)))
#define GB(n) ((BYTES) ((n) * MB(1024)))
#define B_TO_KB(n) ((BYTES) ((n) / 1024))
#define B_TO_MB(n) ((BYTES) ((n) / 1024 / 1024))
#define B_TO_GB(n) ((BYTES) ((n) / 1024 / 1024 / 1024))

enum dax {
	NO_DAX,
	DAX_FIRST_RUN,
	DAX_SECOND_RUN,
};

struct file_interval {
	BYTES off;
	BYTES len;
};

// BYTES get_file_size(FILE *file);
BYTES sample_in_range(BYTES lower, BYTES upper);
// void read_bytes(FILE *file, BYTES bytes, BYTES buffer_size);
void write_bytes_fuse(struct uk_fuse_dev *fusedev, uint64_t nodeid,
		      uint64_t fh, BYTES foffset, BYTES bytes,
		      BYTES buffer_size);
void write_bytes_dax(uint64_t dax_addr, uint64_t moffset, BYTES bytes,
		     BYTES buffer_size);
void read_bytes_fuse(struct uk_fuse_dev *fusedev, uint64_t nodeid,
		     uint64_t fh, BYTES foffset, BYTES bytes,
		     BYTES buffer_size);
void read_bytes_dax(uint64_t dax_addr, uint64_t moffset, BYTES bytes,
		    BYTES buffer_size);
void create_all_files(struct uk_fuse_dev *fusedev, FILES *amount, size_t len,
		      int measurements);
void slice_file(BYTES file_size, struct file_interval **intervals,
		BYTES **interval_order, BYTES *num_intervals,
		BYTES interval_len);
void slice_file_malloc(BYTES file_size, struct file_interval **intervals,
		BYTES **interval_order, BYTES *num_intervals,
		BYTES interval_len);


#endif /* HELPER_FUNCTIONS_H */