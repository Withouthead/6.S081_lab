// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

#define PTE_COW (1L << 8)
#define COW_INDEX(pa) (pa >> 12)
struct pa_struct{
  struct spinlock lock;
} pa_lock;

int pa_cow_count[PHYSTOP/PGSIZE];
void set_cow_count(uint64 pa, int value)
{
  acquire(&pa_lock.lock);
  int pa_index = COW_INDEX(pa);
  pa_cow_count[pa_index] = value;
  release(&pa_lock.lock);
}
void add_cow_count(uint64 pa, int value)
{
  acquire(&pa_lock.lock);
  int pa_index = COW_INDEX(pa);
  pa_cow_count[pa_index] += value;
  release(&pa_lock.lock);
}
int get_cow_count(uint64 pa)
{
  acquire(&pa_lock.lock);
  int pa_index = COW_INDEX(pa);
  release(&pa_lock.lock);
  return pa_cow_count[pa_index];
  
}
struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&pa_lock.lock,"palock");
  initlock(&kmem.lock, "kmem");
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

  // Fill with junk to catch dangling refs.
  acquire(&kmem.lock);
  if(get_cow_count((uint64)pa) > 1)
  {
    
    add_cow_count((uint64)pa, -1);
    release(&kmem.lock);
    return;
  }
  else
    set_cow_count((uint64)pa, 0);

  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  set_cow_count((uint64)r, 1);
  return (void*)r;
}
