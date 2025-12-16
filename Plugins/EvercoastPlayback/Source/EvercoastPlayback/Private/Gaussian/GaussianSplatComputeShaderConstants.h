#pragma once

// GPU SORT ONLY
#define GPU_SORT_BUFFER_COUNT (2)

// Those constants must be kept the same as in GaussianSplatConstants.ush
#define TILE_SIZE (16)
#define BLOCK_SIZE (TILE_SIZE*TILE_SIZE)

#define CALC_VIEW_DATA_THREADS (128)

#define INCLUSIVE_SUM_BLOCK_SIZE (1024)
#define INCLUSIVE_SUM_GROUP_SIZE (1024)

#define DUPLICATE_KEY_THREADS (1024)
#define FIND_RANGE_THREADS (1024)
#define INIT_SORTING_THREADS (128)
#define COPY_SORTING_THREADS (128)

// For groupshared memory in TileRenderer.usf
#define COLLECTOR_BATCH_SIZE (TILE_SIZE*TILE_SIZE)

// 15 per colour channel for SH degree==3
#define MAXIMUM_SH_COEFFS (15)


