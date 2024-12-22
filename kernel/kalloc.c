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

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];//myTODO:和lab3中的思想一致，为每个cpu分配独立的内存申请/释放区，并使用独立的锁进行保护。
//begin myTODO:CPU命名
char *kmem_lock_names[] = 
{
  "kmem_cpu_0",
  "kmem_cpu_1",
  "kmem_cpu_2",
  "kmem_cpu_3",
  "kmem_cpu_4",
  "kmem_cpu_5",
  "kmem_cpu_6",
  "kmem_cpu_7",
};

void
kinit()
{
  // initlock(&kmem.lock, "kmem"); 
  // 进行相应地修改：
  for(int i = 0; i < NCPU ; i++) 
  {
    initlock(&kmem[i].lock, kmem_lock_names[i]);
  }
  freerange(end, (void*)PHYSTOP);
}
//end myTODO

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
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  //begin myTODO

  push_off();//开中断
  int cpu = cpuid();//获取cpu编号

  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);

  pop_off();//关中断
  //end myTODO
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  //begin myTODO：同上，做相应地修改。

  push_off();

  int cpu = cpuid();

  acquire(&kmem[cpu].lock);

  if(!kmem[cpu].freelist) 
  { //如果本cpu没有空闲页
    int borrow_remain = 64; // 向其他的cpu借物理页。这里设置为64
    for(int i=0;i<NCPU;i++)
    {
      if(i == cpu) 
      {      
        continue; //不能自己借自己
      }
      acquire(&kmem[i].lock);
      struct run *rr = kmem[i].freelist;
      while(rr && borrow_remain) 
      {
        kmem[i].freelist = rr->next;
        rr->next = kmem[cpu].freelist;
        kmem[cpu].freelist = rr;
        rr = kmem[i].freelist;
        borrow_remain--;
      }
      release(&kmem[i].lock);
      if(borrow_remain == 0)
      {
        break;
      } //借完
    }
  }

  r = kmem[cpu].freelist;
  if(r)
  {
    kmem[cpu].freelist = r->next;
  }
  release(&kmem[cpu].lock);

  pop_off();

  if(r)
  {
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  return (void*)r;
}
