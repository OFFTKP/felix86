#pragma once

#include "felix86/common/utility.h"

typedef struct backend_block_metadata_s backend_block_metadata_t;

backend_block_metadata_t* backend_block_metadata_create();
void backend_block_metadata_destroy(backend_block_metadata_t* metadata);
void* backend_block_metadata_get_code_ptr(backend_block_metadata_t* metadata, u64 address);
void backend_block_metadata_patch(backend_block_metadata_t* metadata, u64 address,
                                  void* start_of_block);
void backend_block_metadata_unpatch(backend_block_metadata_t* metadata, u64 address,
                                    void* start_of_block);
