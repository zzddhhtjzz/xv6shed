#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sched.h"
#include "sched_fifo.h"
#include "sched_RR.h"


#define _check_curproc(a) do{	\
}while(0)\

/*
#define _check_curproc(a) do{	\
  if(cp == 0){		\
    cprintf("%d, cp == 0\n", a);	\
    panic("");\
  }\
}while(0)\
*/
struct spinlock proc_table_lock;

struct proc proc[NPROC];
struct proc *initproc;
// by jimmy:
struct proc* idleproc[NCPU];
struct rq rq;

int nextpid = 1;
extern void forkret(void);
extern void forkret1(struct trapframe*);

void
pinit(void)
{
  initlock(&proc_table_lock, "proc_table");
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and return it.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  int i;
  struct proc *p;

  acquire(&proc_table_lock);
  for(i = 0; i < NPROC; i++){
    p = &proc[i];
    if(p->state == UNUSED){
      p->state = EMBRYO;
      // by jimmy: currently there is only 1 rq and 1 sched_class, so:
      p->sched_class = (struct sched_class*)&simple_sched_class;
      p->rq = &rq;
      p->pid = nextpid++;
      p->timeslice = INIT_SLICE;
      release(&proc_table_lock);
      return p;
    }
  }
  release(&proc_table_lock);
  return 0;
}

// Grow current process's memory by n bytes.
// Return old size on success, -1 on failure.
int
growproc(int n)
{
  char *newmem;

  newmem = kalloc(cp->sz + n);
  if(newmem == 0)
    return -1;
  memmove(newmem, cp->mem, cp->sz);
  memset(newmem + cp->sz, 0, n);
  kfree(cp->mem, cp->sz);
  cp->mem = newmem;
  cp->sz += n;
  setupsegs(cp);
  return cp->sz - n;
}

// Set up CPU's segment descriptors and task state for a given process.
// If p==0, set up for "idle" state for when scheduler() is running.
void
setupsegs(struct proc *p)
{
  struct cpu *c;
  
  pushcli();
  c = &cpus[cpu()];
  c->ts.ss0 = SEG_KDATA << 3;
  if(p)
    c->ts.esp0 = (uint)(p->kstack + KSTACKSIZE);
  else
    c->ts.esp0 = 0xffffffff;

  c->gdt[0] = SEG_NULL;
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0x100000 + 64*1024-1, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_TSS] = SEG16(STS_T32A, (uint)&c->ts, sizeof(c->ts)-1, 0);
  c->gdt[SEG_TSS].s = 0;
  if(p){
    c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, (uint)p->mem, p->sz-1, DPL_USER);
    c->gdt[SEG_UDATA] = SEG(STA_W, (uint)p->mem, p->sz-1, DPL_USER);
  } else {
    c->gdt[SEG_UCODE] = SEG_NULL;
    c->gdt[SEG_UDATA] = SEG_NULL;
  }

  lgdt(c->gdt, sizeof(c->gdt));
  ltr(SEG_TSS << 3);
  popcli();
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
struct proc*
copyproc(struct proc *p)
{
  int i;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0){
    cprintf("(np = allocproc()) == 0");
    return 0;
  }

  // Allocate kernel stack.
  if((np->kstack = kalloc(KSTACKSIZE)) == 0){
    cprintf("(np->kstack = kalloc(KSTACKSIZE)) == 0");
    np->state = UNUSED;
    return 0;
  }
  np->tf = (struct trapframe*)(np->kstack + KSTACKSIZE) - 1;

  if(p){  // Copy process state from p.
    np->parent = p;
    memmove(np->tf, p->tf, sizeof(*np->tf));
  
    np->sz = p->sz;
    if((np->mem = kalloc(np->sz)) == 0){
      cprintf("(np->mem = kalloc(np->sz)) == 0");
      kfree(np->kstack, KSTACKSIZE);
      np->kstack = 0;
      np->state = UNUSED;
      np->parent = 0;
      return 0;
    }
    memmove(np->mem, p->mem, np->sz);

    for(i = 0; i < NOFILE; i++)
      if(p->ofile[i])
        np->ofile[i] = filedup(p->ofile[i]);
    np->cwd = idup(p->cwd);
  }

  // Set up new context to start executing at forkret (see below).
  memset(&np->context, 0, sizeof(np->context));
  np->context.eip = (uint)forkret;
  np->context.esp = (uint)np->tf;

  // Clear %eax so that fork system call returns 0 in child.
  np->tf->eax = 0;
  return np;
}

/*
// alloc a idle process, just like what is doing up
void
allocidle(int cpuid){
  struct proc* np = &(idleproc[cpuid]);
  if((np->kstack = kalloc(KSTACKSIZE)) == 0){
    np->state = UNUSED;
    return;
  }

  np->tf = (struct trapframe*)(np->kstack + KSTACKSIZE) - 1;

  // Set up new context to start executing at forkret (see below).
  memset(&np->context, 0, sizeof(np->context));
  np->context.eip = (uint)forkret;
  np->context.esp = (uint)np->tf;

  // Clear %eax so that fork system call returns 0 in child.
  np->tf->eax = 0;
  return;
}
*/

struct proc*
allocIdle(void){
  extern uchar _binary_idlecode_start[], _binary_idlecode_size[];

  // init idle proc
  struct proc* p = copyproc(0);
  p->sz = PAGE;
  p->mem = kalloc(p->sz);
  p->cwd = namei("/");
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = p->sz;
  
  // Make return address readable; needed for some gcc.
  p->tf->esp -= 4;
  *(uint*)(p->mem + p->tf->esp) = 0xefefefef;

  // On entry to user space, start executing at beginning of initcode.S.
  p->tf->eip = 0;
  memmove(p->mem, _binary_idlecode_start, (int)_binary_idlecode_size);
    safestrcpy(p->name, "idle", sizeof(p->name));
  p->state = RUNNABLE;

  return p;
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern uchar _binary_initcode_start[], _binary_initcode_size[];
  int i;

  p = copyproc(0);
  p->sz = PAGE;
  p->mem = kalloc(p->sz);
  p->cwd = namei("/");
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = p->sz;
  
  // Make return address readable; needed for some gcc.
  p->tf->esp -= 4;
  *(uint*)(p->mem + p->tf->esp) = 0xefefefef;

  // On entry to user space, start executing at beginning of initcode.S.
  p->tf->eip = 0;
  memmove(p->mem, _binary_initcode_start, (int)_binary_initcode_size);
  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->state = RUNNABLE;
  acquire(&proc_table_lock);
  acquire(&(rq.rq_lock));
  p->sched_class->enqueue_proc(p->rq, p);
  release(&(rq.rq_lock));
  release(&proc_table_lock);

  initproc = p;

  for(i=0; i<NCPU; i++){
    p = idleproc[i] = allocIdle();
    safestrcpy(p->name, "idle", sizeof(p->name));
    p->name[4] = (char)(i+'0');
    p->name[5] = '\0';
    //p->sched_class->enqueue_proc(p->rq, p);
  }

  return;
}

// Return currently running process.
struct proc*
curproc(void)
{
  struct proc *p;

  pushcli();
  p = cpus[cpu()].curproc;
  popcli();
  return p;
}


// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
runIdle(void)
{
  struct proc *p;
  struct cpu *c;

  c = &cpus[cpu()];
  p = idleproc[cpu()];

  cprintf("begin run first proc in cpu[%d]\n", cpu());//debug

  sti();
  acquire(&proc_table_lock);
  acquire(&(rq.rq_lock));
  c->curproc = p;
  _check_curproc(2);
  setupsegs(p);
  p->state = RUNNING;

  // by jimmy:
  swtch(&c->context, &p->context);
}


// by jimmy:
// schedule is the only interface that can swictch CPU from one
// process another
// must acquire the spinlock of rq before call this and release it 
// after calling
void
schedule(void){
  struct cpu *c;
  struct proc* next, *prev;
//  cprintf("Schedule\n");
  c = &cpus[cpu()];
  prev = cp;

  //if(cp is not running)
  //   remove it from the queue
  if((prev->state != RUNNABLE) && (prev->state != RUNNING))
  {
    prev->sched_class->dequeue_proc(prev->rq, prev);
  }
  // get next proc to run 
  next = prev->sched_class->pick_next_proc(prev->rq);

  // swtch
  c->curproc = next;
  _check_curproc(1);//debug
  setupsegs(next);
  next->state = RUNNING;
  if(next != prev)
  {
    swtch(&prev->context, &next->context);
  }
  // may some bugs???
}

/*
// Enter scheduler.  Must already hold proc_table_lock
// and have changed curproc[cpu()]->state.
void
sched(void)
{
  if(read_eflags()&FL_IF)
    panic("sched interruptible");
  if(cp->state == RUNNING)
    panic("sched running");
  if(!holding(&proc_table_lock))
    panic("sched proc_table_lock");
  if(cpus[cpu()].ncli != 1)
    panic("sched locks");

  swtch(&cp->context, &cpus[cpu()].context);
}*/

// Give up the CPU for one scheduling round.
void
yield(void)
{
  //lock the rq
  acquire(&proc_table_lock);
  acquire(&(cp->rq->rq_lock));
  cp->state = RUNNABLE;

  //sched();
  // by jimmy:
  cp->sched_class->yield_proc(cp->rq);
  schedule();

  // unlock rq
  release(&(cp->rq->rq_lock));
  release(&proc_table_lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  // Still holding proc_table_lock from scheduler.
  release(&(cp->rq->rq_lock));
  release(&proc_table_lock);

  // Jump into assembly, never to return.
  forkret1(cp->tf);
}

// Atomically release lock and sleep on chan.
// Reacquires lock when reawakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(cp == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire proc_table_lock in order to
  // change p->state and then call sched.
  // Once we hold proc_table_lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with proc_table_lock locked),
  // so it's okay to release lk.
  if(lk == &proc_table_lock){
    acquire(&(cp->rq->rq_lock));
  }else if(lk == &(cp->rq->rq_lock)){
    acquire(&proc_table_lock);
  }else{
    acquire(&proc_table_lock);
    acquire(&(cp->rq->rq_lock));
    release(lk);
  }

  // Go to sleep.
  cp->chan = chan;
  cp->state = SLEEPING;
  //sched();
  schedule();	// by jimmy

  // Tidy up.
  cp->chan = 0;

  // Reacquire original lock.
  if(lk == &proc_table_lock){
    release(&(cp->rq->rq_lock));
  }else if(lk == &(cp->rq->rq_lock)){
    release(&proc_table_lock);
  }else{
    release(&proc_table_lock);
    release(&(cp->rq->rq_lock));
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// Proc_table_lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;
      p->sched_class->enqueue_proc(p->rq, p);
    }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  // debug
  _check_curproc(3);

  // by jimmy: lock the rq
  acquire(&proc_table_lock);
  acquire(&(rq.rq_lock));

  wakeup1(chan);

  // by jimmy: unlock rq
  release(&(rq.rq_lock));
  release(&proc_table_lock);
}

// Kill the process with the given pid.
// Process won't actually exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&proc_table_lock);
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING){
        p->state = RUNNABLE;
        p->sched_class->enqueue_proc(p->rq, p);
      }
      release(&proc_table_lock);
      return 0;
    }
  }
  release(&proc_table_lock);
  return -1;
}

// Exit the current process.  Does not return.
// Exited processes remain in the zombie state
// until their parent calls wait() to find out they exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(cp == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(cp->ofile[fd]){
      fileclose(cp->ofile[fd]);
      cp->ofile[fd] = 0;
    }
  }

  iput(cp->cwd);
  cp->cwd = 0;

  // lock rq
  acquire(&(cp->rq->rq_lock));
  acquire(&proc_table_lock);

  // Parent might be sleeping in wait().
  wakeup1(cp->parent);

  // Pass abandoned children to init.
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->parent == cp){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  cp->killed = 0;
  cp->state = ZOMBIE;
  schedule();	//by jimmy
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int i, havekids, pid;

  acquire(&proc_table_lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(i = 0; i < NPROC; i++){
      p = &proc[i];
      if(p->state == UNUSED)
        continue;
      if(p->parent == cp){
        if(p->state == ZOMBIE){
          // Found one.
          kfree(p->mem, p->sz);
          kfree(p->kstack, KSTACKSIZE);
          pid = p->pid;
          p->state = UNUSED;
          p->pid = 0;
          p->parent = 0;
          p->name[0] = 0;
          release(&proc_table_lock);
          return pid;
        }
        havekids = 1;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || cp->killed){
      release(&proc_table_lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(cp, &proc_table_lock);
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i, j;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(i = 0; i < NPROC; i++){
    p = &proc[i];
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context.ebp+2, pc);
      for(j=0; j<10 && pc[j] != 0; j++)
        cprintf(" %p", pc[j]);
    }
    cprintf("\n");
  }
}

