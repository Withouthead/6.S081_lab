#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "proc.h"
#include "fcntl.h"

uint64
sys_exit(void)
{
  int n;
  if (argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if (argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_mmap(void)
{
  //TODO: finish it
  uint64 addr = 0;
  int len = 0;
  int prot = 0;
  int flag = 0;
  int fd = 0;
  int offset = 0;
  struct file *mapped_file;
  struct proc *p = myproc();

  if (argaddr(0, &addr) < 0 || argint(1, &len) < 0 || argint(2, &prot) < 0 || argint(3, &flag) < 0 || argfd(4, &fd, &mapped_file) < 0 || argint(5, &offset) < 0)
    return -1;

  if (mapped_file->writable == 0 && (prot & PROT_WRITE) && (flag & MAP_SHARED))
    return -1;
  len = PGROUNDUP(len);
  struct vma *free_vma = find_free_vma();

  if (free_vma == 0)
    return -1;

  free_vma->vm_file = filedup(mapped_file);
  free_vma->size = len;
  free_vma->flag = flag;
  free_vma->prot = prot;
  free_vma->offet = offset;
  free_vma->va_start = p->sz;

  p->sz += len;

  return free_vma->va_start;
}
uint64
sys_munmap(void)
{
  uint64 addr;
  int len;
  struct proc *p = myproc();
  if (argaddr(0, &addr) < 0)
    return -1;
  if (argint(1, &len) < 0)
    return -1;

  addr = PGROUNDDOWN(addr);
  len = PGROUNDUP(len);
  for (int i = 0; i < VMASIZE; i++)
  {
    struct vma *v = &p->vma_list[i];
    if (v->used == 1 && v->va_start <= addr && v->va_start < v->va_start + v->size)
    {
      int offset = v->offet;
      int close = 0;
      if (addr == v->va_start)
      {

        if (len >= v->size)
        {
          len = PGROUNDUP(v->size);
          v->used = 0;
          close = 1;
        }

        else
        {

          v->va_start += len;
          v->offet += len;
        }
      }

      if (v->flag & MAP_SHARED)
      {
        if (filewrite_offset(v->vm_file, offset, addr, len) < 0)
        {
          return -1;
        }
      }

      int npage = len / PGSIZE;

      v->size -= len;
      p->sz -= len;
      uvmunmap(p->pagetable, addr, npage, 0);
      if (close)
      {
        fileclose(v->vm_file);
      }
      return 0;
    }
  }

  return -1;
}