#include "felix86/backend/block_metadata.h"

#include <tsl/robin_map.h>

#include <vector>

struct backend_block_metadata_s {
    tsl::robin_map<u64, void*> backend_block_map;

    struct patch_location_t {
        std::vector<void*> jg;
    };

    tsl::robin_map<u64, patch_location_t> patch_map;
};

backend_block_metadata_t* backend_block_metadata_create() {
    return new backend_block_metadata_t();
}

void backend_block_metadata_destroy(backend_block_metadata_t* metadata) {
    delete metadata;
}

void* backend_block_metadata_get_code_ptr(backend_block_metadata_t* metadata, u64 address) {
    auto it = metadata->backend_block_map.find(address);
    if (it == metadata->backend_block_map.end()) {
        return nullptr;
    }

    return it->second;
}

void backend_block_metadata_patch(backend_block_metadata_t* metadata, u64 address,
                                  void* start_of_block) {
    auto& patch_location = metadata->patch_map[address];
}