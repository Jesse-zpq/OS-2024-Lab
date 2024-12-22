// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUFMAP_BUCKET 13
#define BUFMAP_HASH(dev, blockno) ((((dev)<<27)|(blockno))%NBUFMAP_BUCKET)

struct {
  struct spinlock eviction_lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf bufmap[NBUFMAP_BUCKET];
  struct spinlock bufmap_locks[NBUFMAP_BUCKET];
} bcache;

void
binit(void)
{
  // struct buf *b;

  // initlock(&bcache.lock, "bcache");

  // // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
  //begin myTODO:binit逻辑改变，需要重构
  //
  for(int i=0;i<NBUFMAP_BUCKET;i++)
  {
    initlock(&bcache.bufmap_locks[i], "bcache_bufmap");
    bcache.bufmap[i].next = 0;
  }

  for(int i=0;i<NBUF;i++)
  {
    struct buf *b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    b->lastuse = 0;
    b->refcnt = 0;
    // put all the buffers into bufmap[0]
    b->next = bcache.bufmap[0].next;
    bcache.bufmap[0].next = b;
  }
  //end myTODO

  initlock(&bcache.eviction_lock, "bcache_eviction");

}


/// @brief myTODO：
///重点内容来了，bget函数将要实现缓存的获取。这需要细致选取每个cpu释放锁的时机，避免一个cpu释放锁后，另一个cpu认为这块内存可以被分配，从而造成潜在的覆写问题。
///受到网上资源的启发，cpu在找到合适内存时，可以并不立刻释放锁，而是等到buffer真正被分配(一般是kalloc)后才释放.
///这条思路顺便解决了死锁问题:两个cpu c1 c2 可能想要申请由 c2 c1 管理的桶内内存,破坏了死锁四大条件中的"持有请求".
///还有更多此方面的复杂优化与讨论.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint key = BUFMAP_HASH(dev, blockno);

  acquire(&bcache.bufmap_locks[key]);

  // Is the block already cached?
  for(b = bcache.bufmap[key].next; b; b = b->next)
  {
    if(b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      release(&bcache.bufmap_locks[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.bufmap_locks[key]);

  acquire(&bcache.eviction_lock);

  for(b = bcache.bufmap[key].next; b; b = b->next)
  {
    if(b->dev == dev && b->blockno == blockno)
    {
      acquire(&bcache.bufmap_locks[key]); //refcnt修改必须上锁
      b->refcnt++;
      release(&bcache.bufmap_locks[key]);
      release(&bcache.eviction_lock);
      acquiresleep(&b->lock);
      return b;
    }  
  }
  
  struct buf *before_least = 0; 
  uint holding_bucket = -1;
  for(int i = 0; i < NBUFMAP_BUCKET; i++)
  {
    // 这里不会发生循环等待
    acquire(&bcache.bufmap_locks[i]);
    int newfound = 0; // 寻找最久没有被使用的内存
    for(b = &bcache.bufmap[i]; b->next; b = b->next) 
    {
      if(b->next->refcnt == 0 && (!before_least || b->next->lastuse < before_least->next->lastuse)) 
      {
        before_least = b;
        newfound = 1;
      }
    }
    if(!newfound)
    {
      release(&bcache.bufmap_locks[i]);
    } 
    else
    {
      if(holding_bucket != -1) release(&bcache.bufmap_locks[holding_bucket]);
      holding_bucket = i;
    }
  }
  if(!before_least) 
  {
    panic("bget: no buffers");
  }
  b = before_least->next;
  
  if(holding_bucket != key) 
  {
 
    before_least->next = b->next;
    release(&bcache.bufmap_locks[holding_bucket]);

    acquire(&bcache.bufmap_locks[key]);
    b->next = bcache.bufmap[key].next;
    bcache.bufmap[key].next = b;
  }
  
  b->dev = dev;
  b->blockno = blockno;
  b->refcnt = 1;
  b->valid = 0;
  release(&bcache.bufmap_locks[key]);
  release(&bcache.eviction_lock);
  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.

/// @brief myTODO:相应地需要重写
/// @param b 
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  // acquire(&bcache.lock);
  // b->refcnt--;
  // if (b->refcnt == 0) {
  //   // no one is waiting for it.
  //   b->next->prev = b->prev;
  //   b->prev->next = b->next;
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
  
  // release(&bcache.lock);
  uint key = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  if (b->refcnt == 0) 
  {
    b->lastuse = ticks;
  }
  release(&bcache.bufmap_locks[key]);
}
/// @brief myTODO:相应简单修改
/// @param b 
void
bpin(struct buf *b) 
{
  uint key = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_locks[key]);
  b->refcnt++;
  release(&bcache.bufmap_locks[key]);
}
/// @brief myTODO:相应简单修改
/// @param b 
void
bunpin(struct buf *b) 
{
  uint key = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  release(&bcache.bufmap_locks[key]);
}


