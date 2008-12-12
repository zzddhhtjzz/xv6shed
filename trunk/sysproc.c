#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sched.h"

int
sys_fork(void)
{
  int pid;
  struct proc *np;

  //cprintf("in sysfork: %s\n", cp->name);
  if((np = copyproc(cp)) == 0){
    cprintf("sysfork failed here\n");
    return -1;
  }
  acquire(&proc_table_lock);
  acquire(&(cpus[cpu()].rq->rq_lock));
  pid = np->pid;
  np->state = RUNNABLE;
  enqueue_proc(cpus[cpu()].rq, np);
  release(&(cpus[cpu()].rq->rq_lock));
  release(&proc_table_lock);
  return pid;
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
//  cprintf("sys_getpid: [%s]\n", cp->name);
  return cp->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  if((addr = growproc(n)) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n, ticks0;
  
  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(cp->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}
