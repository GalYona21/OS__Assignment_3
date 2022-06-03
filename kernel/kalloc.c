// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

//pages reference array
int refs[NUM_PYS_PAGES];
struct spinlock lock_ref;

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
extern uint64 cas(volatile void *addr,int expected,int newval);


struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

int
get_ref_index(void* pa){
    return ((uint64)pa) / PGSIZE;
}

int
ref_dec(uint64 pa){
    int curr_ref;
//    printf("ref_dec 1 refs[PA2PTE(pa)]: %p", refs[PA2PTE(pa)]);
//    do{
//        curr_ref = refs[PA2PTE(pa)];
//    }while(cas(&refs[PA2PTE(pa)], refs[PA2PTE(pa)], refs[PA2PTE(pa)]-1));
    acquire(&lock_ref);
    curr_ref = --refs[get_ref_index((void*)pa)];
    release(&lock_ref);
//    printf("ref_dec 2");

    return curr_ref;
}

int
ref_inc(uint64 pa){
    int curr_ref;
//    printf("ref_inc 1");

//    do{
//        curr_ref = refs[PA2PTE(pa)];
//    }while(cas(&refs[PA2PTE(pa)], refs[PA2PTE(pa)], refs[PA2PTE(pa)]+1));
    acquire(&lock_ref);
    curr_ref = ++refs[get_ref_index((void*)pa)];
    release(&lock_ref);
//    printf("ref_inc 2");

    return curr_ref;
}

int
get_ref(uint64 pa){
    return refs[get_ref_index((void*)pa)];
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
    initlock(&lock_ref, "lock_ref");
// initialize refs array with 0's
  memset(refs, 0, sizeof(int)*NUM_PYS_PAGES);
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if(ref_dec((uint64)pa) > 0){
//      printf("kfree 1");

      return;
  }

  refs[get_ref_index((void*)pa)] = 0;
//  printf("kfree 2");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
//    printf("kfree 2");

}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
//    printf("kalloc");

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
      refs[get_ref_index((void*)r)] = 1;
      kmem.freelist = r->next;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}


