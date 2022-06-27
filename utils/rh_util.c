#include <assert.h>
#include <sys/sysinfo.h>
#include <asm/unistd.h>
#include <fcntl.h>
#include "rh_util.h"

void clflush(const void *addr) {
  asm volatile("clflush (%0)" : : "r" (addr));
}

//cpuid serailizes the instruction queue
uint64_t rdtsc() {
  // Source: https://github.com/IAIK/rowhammerjs
  uint64_t a, d;
  asm volatile ("cpuid" ::: "rax","rbx","rcx","rdx");
  asm volatile ("rdtscp" : "=a" (a), "=d" (d) : : "rcx");
  a = (d<<32) | a;
  return a;
}
uint64_t rdtsc2() {
  // Source: https://github.com/IAIK/rowhammerjs
  uint64_t a, d;
  asm volatile ("rdtscp" : "=a" (a), "=d" (d) : : "rcx");
  asm volatile ("cpuid" ::: "rax","rbx","rcx","rdx");
  a = (d<<32) | a;
  return a;
}

void flush_page(uint64_t* va){
  for (uint32_t index = 0; index < 512; ++index){

    //flush it out to memory
    //assume page address is cache line aligned
    if((index+1)%8 == 0){// 0-7  first cache line, 8-15 second...
      clflush(va+index);
    }
  }
}

void set_and_flush(uint64_t* va, uint64_t value){
  for (uint32_t index = 0; index < 512; ++index){
    va[index] = value;
    /*TODO, if we store and cflush one by one, the cache line is not
    guranteed be flushed to DRAM. This may involve how clflush is ordered,
    which is worth to be explored later*/
    //asm volatile ("mfence" ::: "memory");
    // if(index%7 == 0){// 0-7  first cache line, 8-15 second...
    //   clflush(va+index);
    // }
  }
  asm volatile("mfence");//this may not be necessary
  flush_page(va);
}

void force_exit(int sig){
#ifdef SCAN_ALL
  fclose(fp_map);
#endif
  exit(-1);
}

// Obtain the size of the physical memory of the system.
uint64_t GetPhysicalMemorySize() {
  struct sysinfo info;
  sysinfo( &info );
  return (size_t)info.totalram * (size_t)info.mem_unit;
}

uint64_t get_physical_addr(uint64_t virtual_addr) {
  // Source: https://github.com/IAIK/rowhammerjs
  uint64_t value;
  int pagemap = open("/proc/self/pagemap", O_RDONLY);
  assert(pagemap >= 0);
  off_t offset = (virtual_addr / 4096) * sizeof(value);
  int got = pread(pagemap, &value, sizeof(value), offset);
  assert(got == 8);

  // Check the "page present" flag.
  assert(value & (1ULL << 63));

  uint64_t frame_num = value & ((1ULL << 54) - 1);
  return (frame_num * 4096) | (virtual_addr & (4095));
}

uint64_t get_pa(uint64_t* virtual_addr) {
  /*
   * Given the virtual address, 
   * returns the physical page frame number
   *
   * Usage:
   * uint8_t buffer = static_cast<uint8_t*>(mmap(NULL, <size>, \
   * PROT_READ | PROT_WRITE, MAP_POPULATE | MAP_PRIVATE | \
   * MAP_ANONYMOUS, 0, 0));
   * get_page_frame_number(buffer);
   */
  int pagemap = open("/proc/self/pagemap", O_RDONLY);
  int64_t value;
  int got = pread(pagemap, &value, 8,
      ((uintptr_t)(virtual_addr) / 0x1000) * 8);
  assert(got == 8);
  // Check the "page present" flag.
  assert(value & (1ULL << 63));

  uint64_t frame_num = value & ((1ULL << 54) - 1);
  close(pagemap);
  return (frame_num * 4096) | ((uint64_t)virtual_addr & (4095));
}


// uint64_t GetPageFrameNumber(int pagemap, uint8_t* virtual_address) {
//   // Read the entry in the pagemap.
//   uint64_t value;
//   int pagemap = open("/proc/self/pagemap", O_RDONLY);
//   assert(pagemap >= 0);
//   int got = pread(pagemap, &value, 8,
//       (reinterpret_cast<uintptr_t>(virtual_address) / 0x1000) * 8);
//   assert(got == 8);

//   // Check the "page present" flag.
//   assert(value & (1ULL << 63));
//   uint64_t page_frame_number = value & ((1ULL << 54)-1);
//   return page_frame_number;
// }



//#define MEASURE_EVICTION
uint64_t HammerAddressesStandard(
    const std::pair<uint64_t, uint64_t>& first_range,
    const std::pair<uint64_t, uint64_t>& second_range,
    uint64_t number_of_reads) {
  // Source: https://github.com/IAIK/rowhammerjs

  size_t histf[1500];
  size_t hists[1500];

  #define ADDR_COUNT (32)

  volatile uint64_t* faddrs[ADDR_COUNT];
  volatile uint64_t* saddrs[ADDR_COUNT];

  faddrs[0] = (uint64_t*) first_range.first;
  saddrs[0] = (uint64_t*) second_range.first;

  volatile uint64_t* f = faddrs[0];
  volatile uint64_t* s = saddrs[0];

  uint64_t sum = 0;
  size_t t0 = rdtsc();
  size_t t = 0,t2 = 0,delta = 0,delta2 = 0;

#ifdef TEST_HAMMER_TIME
  struct timespec start, end;
  //get start time
  clock_gettime(CLOCK_MONOTONIC, &start);
#endif

  while (number_of_reads-- > 0) {
#ifdef MEASURE_EVICTION
    rdtsc();
    t = rdtsc();
#endif
    *f;
#ifdef MEASURE_EVICTION
    t2 = rdtsc2();
    histf[MAX(0,MIN(99,(t2 - t) / 5))]++;
    rdtsc2();
    rdtsc();
    t = rdtsc();
#endif
    *s;
#ifdef MEASURE_EVICTION
    t2 = rdtsc2();
    hists[MAX(MIN((t2 - t) / 5,99),0)]++;
#endif

#ifdef EVICTION_BASED
    for (size_t i = 1; i < 18; i += 1)
    {
      *faddrs[i];
      *saddrs[i];
      *faddrs[i+1];
      *saddrs[i+1];
      *faddrs[i];
      *saddrs[i];
      *faddrs[i+1];
      *saddrs[i+1];
      *faddrs[i];
      *saddrs[i];
      *faddrs[i+1];
      *saddrs[i+1];
      *faddrs[i];
      *saddrs[i];
      *faddrs[i+1];
      *saddrs[i+1];
      *faddrs[i];
      *saddrs[i];
      *faddrs[i+1];
      *saddrs[i+1];
    }
#else
#if defined(SKYLAKE) && !defined(NO_CLFLUSHOPT)
    asm volatile("clflushopt (%0)" : : "r" (f) : "memory");
    asm volatile("clflushopt (%0)" : : "r" (s) : "memory");
#else
    //prior work (e.g., one bit flop in cloud) indicates mfence is necessary
    asm volatile("clflush (%0)" : : "r" (f) : "memory");
    asm volatile("clflush (%0)" : : "r" (s) : "memory");
#endif
#endif
  }

#ifdef TEST_HAMMER_TIME
  clock_gettime(CLOCK_MONOTONIC, &end);
  double time_taken; 
  time_taken = (end.tv_sec - start.tv_sec) * 1e3 + (end.tv_nsec - start.tv_nsec) * 1e-6;
  fprintf(stdout, "time taken for hammering:%.2fms\n", time_taken);
#endif

  //print average latency for each read
  //printf("%zu ",(rdtsc2() - t0) / (NUMBER_OF_READS));
#ifdef MEASURE_EVICTION
  for (size_t i = 0; i < 100; ++i)
  {
    printf("%zu,%zu\n",i * 5, histf[i] + hists[i]);
    histf[i] = 0;
    hists[i] = 0;
  }
#endif
  //dummy(f0,f1,s0,s1);
  return sum;
}