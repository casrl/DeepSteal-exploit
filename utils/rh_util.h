#ifndef RH_UTIL
#define RH_UTIL

#include <iostream>
#include <unistd.h>
#include <map>
#include <vector>
#include <utility>

enum FLIP_TYPE{
  /*two dimensions: strong means need low-high-low; weak means low-high-high | high-high-low
    Also, the 3 rows may cross scrambling boundary. The boundary could be at the first row
    --> YYX or second row --> XYY. 
  */

  /* the combinations are as follows
  Give the following test patterns (column wise)
  0 1 0 1
  1 1 1 1
  0 0 1 1
  //+ usable, +* usable with opposite setting for agg2, - not usable
  Strong, No-crossing:               Y N N N                 +
  Strong, crossing (2nd):            N N Y N                 +*
  Strong, crossing (1st):            N Y N N                 +

  Weak, No-crossing, (either):       Y Y Y N                 +*
  Weak, No-crossing, (upper only):   Y N Y N <- Ambiguous 1  +
  Weak, No-crossing, (lower only):   Y Y N N <- Ambiguous 2  -

  Weak, crossing (2nd), (either):    Y N Y Y                 +
  Weak, crossing (2nd), (upper only):Y N Y N <- Ambiguous 1  +
  Weak, crossing (2nd), (lower only):N N Y Y                 -

  Weak, crossing (1st), (either):    Y Y N Y                 +*
  Weak, crossing (1st), (upper only):N Y N Y                 +
  Weak, crossing (1st), (lower only):Y Y N N <- Ambiguous 2  -

  */
  STRONG_XYX = 0b1000, //+
  STRONG_XYY = 0b0010,
  STRONG_YYX = 0b0100, //+
  WEAK_XYX_ANY = 0b1110,
  WEAK_XYX_UP = 0b1010, //<- Ambiguous 1 +
  WEAK_XYX_LOW = 0b1100, //<- Ambiguous 2
  WEAK_XYY_ANY = 0b1011, //+
  WEAK_XYY_UP = 0b1010, //<- Ambiguous 1 +
  WEAK_XYY_LOW = 0b0011,
  WEAK_YYX_ANY = 0b1101,
  WEAK_YYX_UP = 0b0101, //+
  WEAK_YYX_LOW = 0b1100, //<- Ambiguous 2
  UNEXPECTED = 0b0
};

// This vector will be filled with all the pages we can get access to for a
// given row size.
typedef std::map<size_t, std::map<uint64_t, std::vector<uint8_t*>>> HASH_ROW_PAGES_T;
typedef std::map<int, std::map<int, int>>  RH_PROFILE_T; //goal, offset, frequency
typedef std::map<int, std::map<int, FLIP_TYPE >>  RH_LEAK_T;
typedef std::map<uint8_t*, RH_PROFILE_T> LEAK_PAGE_OFF_T;

uint64_t rdtsc(); 

uint64_t rdtsc2();

void clflush(const void *addr);

void flush_page(uint64_t* va);

void set_and_flush(uint64_t* va, uint64_t value);

void force_exit(int sig);

uint64_t GetPhysicalMemorySize();

size_t get_dram_mapping(void* phys_addr_p);

uint64_t get_physical_addr(uint64_t virtual_addr);
uint64_t get_pa(uint64_t* virtual_address);

typedef uint64_t(HammerFunction)(
    const std::pair<uint64_t, uint64_t>& first_range,
    const std::pair<uint64_t, uint64_t>& second_range,
    uint64_t number_of_reads);

//#define MEASURE_EVICTION
uint64_t HammerAddressesStandard(
    const std::pair<uint64_t, uint64_t>& first_range,
    const std::pair<uint64_t, uint64_t>& second_range,
    uint64_t number_of_reads);

#endif