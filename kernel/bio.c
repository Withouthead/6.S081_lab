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

#define BUCKET_SIZE 13

struct
{
  struct spinlock lock;

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf *buf_list;
} bcache[BUCKET_SIZE];

struct buf global_buf_list[NBUF];
int global_buf_list_index = 0;
struct spinlock global_va_lock;

int hash_fun(uint index)
{
  return index % BUCKET_SIZE;
}

struct buf *find_buf(uint dev, uint blockno)
{
  int hash_index = hash_fun(blockno);
  struct buf *b = bcache[hash_index].buf_list;
  while (b && (b->blockno != blockno || b->dev != dev))
  {
    b = b->next;
  }
  return b;
}

void binit(void)
{
  struct buf *b;

  initlock(&global_va_lock, "bcache_global_va_lock");
  for (int i = 0; i < BUCKET_SIZE; i++)
  {
    bcache[i].buf_list = 0;
    char lock_name[10];
    snprintf(lock_name, 10, "bcache_%d", i);
    initlock(&bcache[i].lock, lock_name);
  }

  // Create linked list of buffers
  for (int i = 0; i < NBUF; i++)
  {
    b = global_buf_list + i;
    b->time_stamp = 0;
    b->refcnt = 0;
    b->is_release = 1;
    initsleeplock(&b->lock, "buffer");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.

static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b;
  int hash_index = hash_fun(blockno);

  acquire(&bcache[hash_index].lock);

  // Is the block already cached?
  for (b = bcache[hash_index].buf_list; b != 0; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;

      release(&bcache[hash_index].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache[hash_index].lock);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  acquire(&global_va_lock); //TODO: 记得释放
  acquire(&bcache[hash_index].lock);
  if (global_buf_list_index < NBUF)
  {
    b = &global_buf_list[global_buf_list_index];
    b->next = bcache[hash_index].buf_list;
    if (bcache[hash_index].buf_list)
      bcache[hash_index].buf_list->prev = b;
    bcache[hash_index].buf_list = b;

    b->dev = dev;
    b->is_release = 0;
    b->blockno = blockno;
    b->valid = 0;
    b->time_stamp = ticks;
    b->refcnt = 1;
    global_buf_list_index++;
    release(&bcache[hash_index].lock);
    release(&global_va_lock);

    acquiresleep(&b->lock);
    return b;
  }
  b = 0;
  uint min_time = ticks;
  int min_index = 0;
  for (int i = 0; i < BUCKET_SIZE; i++)
  {
    if (i != hash_index)
      acquire(&bcache[i].lock);
    struct buf *temp_b = bcache[i].buf_list;
    while (temp_b)
    {
      if (temp_b->is_release && temp_b->refcnt <= 0 && temp_b->time_stamp < min_time)
      {

        min_index = i;
        b = temp_b;
        min_time = temp_b->time_stamp;
      }
      temp_b = temp_b->next;
    }
    if (i != hash_index)
      release(&bcache[i].lock);
  }
  if (b)
  {
    // printf("%d\n", min_index);
    if (min_index != hash_index)
      acquire(&bcache[min_index].lock);
    struct buf *temp_b;
    temp_b = b->next;
    if (temp_b)
    {
      temp_b->prev = b->prev;
    }
    if (b->prev)
    {
      b->prev->next = temp_b;
    }
    else
    {
      bcache[min_index].buf_list = temp_b;
    }

    b->prev = 0;
    b->next = bcache[hash_index].buf_list;
    if (bcache[hash_index].buf_list)
      bcache[hash_index].buf_list->prev = b;
    bcache[hash_index].buf_list = b;
    b->dev = dev;
    b->blockno = blockno;
    b->refcnt = 1;
    b->valid = 0;
    b->time_stamp = ticks;
    b->is_release = 0;

    if (min_index != hash_index)
      release(&bcache[min_index].lock);
    release(&bcache[hash_index].lock);
    release(&global_va_lock);
    acquiresleep(&b->lock);
    return b;
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  // printf("hello!\n");
  releasesleep(&b->lock);

  int hash_index = hash_fun(b->blockno);
  acquire(&bcache[hash_index].lock);
  b->refcnt--;
  if (b->refcnt <= 0)
  {
    b->is_release = 1;
    // b->valid = 0;
    // memset(b->data, 0, sizeof(b->data));
    // b->valid = 0;
    // no one is waiting for it.
    // struct buf* temp_b;
    // temp_b = b->next;
    // if(temp_b)
    // {
    //   temp_b->prev = b->prev;
    // }
    // if(b->prev)
    // {
    //   b->prev->next = temp_b;
    // }
    // else
    // {
    //   bcache[hash_index].buf_list = temp_b;
    // }

    b->time_stamp = ticks - 1;
  }

  release(&bcache[hash_index].lock);
}

void bpin(struct buf *b)
{
  int hash_index = hash_fun(b->blockno);
  acquire(&bcache[hash_index].lock);
  b->refcnt++;
  release(&bcache[hash_index].lock);
}

void bunpin(struct buf *b)
{
  int hash_index = hash_fun(b->blockno);
  acquire(&bcache[hash_index].lock);
  b->refcnt--;
  release(&bcache[hash_index].lock);
}
