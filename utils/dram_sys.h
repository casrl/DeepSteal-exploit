
#include "rh_util.h"

uint64_t get_row_size();
size_t get_dram_mapping(void* phys_addr_p);
void init_dram_config(size_t dimms);
uint64_t get_pages_in_row();
void arrange_addresses(void* memory_mapping,
uint64_t memory_mapping_size, HASH_ROW_PAGES_T& hash_row_pages);
