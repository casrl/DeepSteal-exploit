#include <iostream>
#include <unistd.h>
#include <map>
#include <vector>
#include <utility>
#include "rh_util.h"

size_t DIMMS = 0;
size_t ranks_per_dimm = 2;
size_t banks_per_rank = 8;
size_t bytes_in_row = 1024*8; //bytes for a row in a bank
uint64_t pages_in_row;
size_t page_span_row = 0;
size_t ROW_SIZE;

size_t get_dram_mapping(void* phys_addr_p) {
  // Source: https://github.com/IAIK/rowhammerjs
  size_t single_dimm_shift = 0;
  if (DIMMS == 1)
    single_dimm_shift = 1;
#if defined(SANDY) || defined(IVY) || defined(HASWELL) || defined(SKYLAKE)
  uint64_t phys_addr = (uint64_t) phys_addr_p;

#if defined(SANDY)
#define ARCH_SHIFT (1)
  static const size_t h0[] = { 14, 18 }; //BA0
  static const size_t h1[] = { 15, 19 }; //BA1
  static const size_t h2[] = { 16, 20 }; //BA2
  static const size_t h3[] = { 17, 21 }; //rank?
  static const size_t h4[] = { 17, 21 };
  static const size_t h5[] = { 6 }; //channel bit


#elif defined(IVY) || defined(HASWELL)
#define ARCH_SHIFT (1)
  static const size_t h0[] = { 14, 18 }; //BA0
  static const size_t h1[] = { 15, 19 }; //BA1
  static const size_t h2[] = { 16, 20 }; //rank
  static const size_t h3[] = { 17, 21 }; //BA2
  static const size_t h4[] = { 17, 21 };
  static const size_t h5[] = { 7, 8, 9, 12, 13, 18, 19 }; //channel bit

#elif defined(SKYLAKE)
#define ARCH_SHIFT (2)
  static const size_t h0[] = { 7, 14 }; //BG0
  static const size_t h1[] = { 15, 19 }; //BG1
  static const size_t h2[] = { 16, 20 }; //rank
  static const size_t h3[] = { 17, 21 }; //BA0
  static const size_t h4[] = { 18, 22 }; //BA1
  static const size_t h5[] = { 8, 9, 12, 13, 18, 19 }; //channel bit

#endif

  size_t hash = 0;
  size_t count = sizeof(h0) / sizeof(h0[0]);
  for (size_t i = 0; i < count; i++) {
    hash ^= (phys_addr >> (h0[i] - single_dimm_shift)) & 1;
  }
  size_t hash1 = 0;
  count = sizeof(h1) / sizeof(h1[0]);
  for (size_t i = 0; i < count; i++) {
    hash1 ^= (phys_addr >> (h1[i] - single_dimm_shift)) & 1;
  }
  size_t hash2 = 0;
  count = sizeof(h2) / sizeof(h2[0]);
  for (size_t i = 0; i < count; i++) {
    hash2 ^= (phys_addr >> (h2[i] - single_dimm_shift)) & 1;
  }
  size_t hash3 = 0;
  count = sizeof(h3) / sizeof(h3[0]);
  for (size_t i = 0; i < count; i++) {
    hash3 ^= (phys_addr >> (h3[i] - single_dimm_shift)) & 1;
  }
  size_t hash4 = 0;
  count = sizeof(h4) / sizeof(h4[0]);
  for (size_t i = 0; i < count; i++) {
    hash4 ^= (phys_addr >> (h4[i] - single_dimm_shift)) & 1;
  }
  size_t hash5 = 0;
  if (DIMMS == 2)
  {
    count = sizeof(h5) / sizeof(h5[0]);
    for (size_t i = 0; i < count; i++) {
      hash5 ^= (phys_addr >> h5[i]) & 1;
    }
  }
  return (hash5 << 5) | (hash4 << 4) | (hash3 << 3) | (hash2 << 2) | (hash1 << 1) | hash;
#else
#define ARCH_SHIFT (1)
  return 0;
#endif
}

void init_dram_config(size_t dimms){
  DIMMS =  dimms;
  ranks_per_dimm = 2;
  banks_per_rank = 8;
  bytes_in_row = 1024*8; //bytes for a row in a bank

  #if defined(SKYLAKE)
    if (DIMMS == 1)
      page_span_row = 2;
    else
      page_span_row = 4;
  #else
    if (DIMMS == 1)
      page_span_row = 1;
    else
      page_span_row = 2;
  #endif

  pages_in_row = bytes_in_row / (4096/page_span_row);
  ROW_SIZE = bytes_in_row * banks_per_rank * ranks_per_dimm *DIMMS * ARCH_SHIFT;
}

uint64_t get_pages_in_row(){
  return pages_in_row;
}

void arrange_addresses(void* memory_mapping, 
    uint64_t memory_mapping_size, HASH_ROW_PAGES_T& hash_row_pages){

  //how many pages does each row contain (partial pages)
  printf("arranging addresses to hash_set, rows and pages\n");
  for (uint64_t offset = 0; offset < memory_mapping_size; offset += 0x1000) { // maybe * DIMMS
    uint8_t* virtual_address = static_cast<uint8_t*>(memory_mapping) + offset;
    uint64_t physical_address = get_physical_addr((uint64_t)virtual_address);
    uint64_t presumed_row_index = physical_address / ROW_SIZE;
    std::vector<std::pair<size_t, uint8_t*>> hash_vaddr_pairs;

    size_t hash_set1 = get_dram_mapping((void*)physical_address);
    // printf("phy_addr:0x%lx, hash_set:%zu\n", physical_address, hash_set1);
    hash_vaddr_pairs.push_back(std::make_pair(hash_set1, virtual_address));

#if defined(SANDY)
    //each page spans two rows
    if(DIMMS == 2)
    {
      size_t hash_set2 = hash_set1 ^ (1UL<<5); // hash5
      uint8_t* vaddr2 = (uint8_t*)((uint64_t)virtual_address ^ (1UL<<6)); // bit 6
      hash_vaddr_pairs.push_back(std::make_pair(hash_set2, vaddr2));
    }

#elif defined(IVY) || defined(HASWELL)   
    //each page spans two rows
    if(DIMMS == 2)
    {
      size_t hash_set2 = hash_set1 ^ (1UL<<5); // hash5
      uint8_t* vaddr2 = (uint8_t*)((uint64_t)virtual_address ^ (1UL<<7)); // bit 7
      hash_vaddr_pairs.push_back(std::make_pair(hash_set2, vaddr2));
    }
#elif defined(SKYLAKE)
    if(DIMMS == 1){
      size_t hash_set2 = hash_set1 ^ (1UL); // hash0
      uint8_t* vaddr2 = (uint8_t*)((uint64_t)virtual_address ^ (1UL<<6)); // bit 6
      hash_vaddr_pairs.push_back(std::make_pair(hash_set2, vaddr2));
    }
    else{//DIMM is 2
      size_t hash_set2 = hash_set1 ^ (1UL); // hash0
      uint8_t* vaddr2 = (uint8_t*)((uint64_t)virtual_address ^ (1UL<<7)); // bit 7
      hash_vaddr_pairs.push_back(std::make_pair(hash_set2, vaddr2));

      size_t hash_set3 = hash_set1 ^ (1UL<<5); // hash5
      uint8_t* vaddr3 = (uint8_t*)((uint64_t)virtual_address ^ (1UL<<8)); // bit 8
      hash_vaddr_pairs.push_back(std::make_pair(hash_set3, vaddr3));

      size_t hash_set4 = hash_set1 ^ (1UL) ^ (1UL<<5); // hash0 and hash5
      uint8_t* vaddr4 = (uint8_t*)((uint64_t)virtual_address ^ (1UL<<7) ^ (1UL<<8)); // bit 7 and bit 8
      hash_vaddr_pairs.push_back(std::make_pair(hash_set4, vaddr4));
    }
#endif
    for(auto pair : hash_vaddr_pairs){
      size_t hash_set = pair.first;//hash
      uint8_t* vaddr = pair.second;//virtual address

      hash_row_pages[hash_set][presumed_row_index].push_back(virtual_address);
    }
  }

  // fprintf(stdout, "total hash sets is %zu:\n", hash_row_pages.size());
  // for(auto entry: hash_row_pages){
  //   fprintf(stdout, "(%zu,%zu) ", entry.first, entry.second.size());
  // }

  printf("address rearrangement done\n");

}

uint64_t get_row_size(){
  return ROW_SIZE;
}