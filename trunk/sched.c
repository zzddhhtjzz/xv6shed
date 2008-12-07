#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sched.h"

static char* rq_lock_name;

// by jimmy:
void init_rq_simple(struct rq* rq){
  int i;

  // mark all the nodes is free
  for(i=0; i<NPROC; i++){
    rq->nodes[i].proc = 0;
    rq->nodes[i].next = &(rq->nodes[i]) + 1;
    rq->nodes[i].prev = 0;
    // debug
    //cprintf("rq[%d] = %x\n", i, &(rq->nodes[i]));
  }
  rq->nodes[NPROC-1].next = 0;
  rq->free_list = &(rq->nodes[0]);
  rq->next_to_run = 0;

  // init lock
  initlock(&(rq->rq_lock), rq_lock_name);
}

void enqueue_proc_simple(struct rq *rq, struct proc *p){
  //cprintf("in enqueue: %x, free_list = %x\n", p->pid, rq->free_list);
  //_check_lock(&(rq->rq_lock), "enqueue_proc_simple no lock");
  if(p->rq != rq)
    panic("rq changed!\n");
  if(!holding(&(rq->rq_lock)))
    panic("enqueue_proc_simple no lock");

  // alloc
  if(rq->free_list == 0)
    panic("kernel panic: Do not support procs more than NPROC!(in enqueue_proc_simple)\n");
  struct rq_node* pnode = rq->free_list;
  rq->free_list = pnode->next;

  // insert before next_to_run
  pnode->proc = p;
  if(rq->next_to_run == 0){
    rq->next_to_run = pnode;
    pnode->next = pnode->prev = pnode;
  }else{
    struct rq_node* prev = rq->next_to_run->prev;
    prev->next = pnode;
    pnode->prev = prev;
    rq->next_to_run->prev = pnode;
    pnode->next = rq->next_to_run;
  }
}

void dequeue_proc_simple(struct rq *rq, struct proc *p){
  // by jimmy:
  extern struct proc* idleproc[];//debug
  //cprintf("in dequeue: %x\n", p->pid);
  _check_lock(&(rq->rq_lock), "dequeue_proc_simple no lock");

  // search
  struct rq_node* cnode;
  if(rq->next_to_run->proc == p)
    cnode = rq->next_to_run;
  else{
    cnode = rq->next_to_run->prev;
    while(cnode->proc != p){
      if(cnode == rq->next_to_run){
	if(p == idleproc[cpu()])
          return;
	cprintf("proc: %s, state = %d\n", p->name, p->state);
        panic("Kernel panic: Cannot find proc in dequeue_proc_simple\n");
      }
      cnode = cnode->prev;
    }
  }

//  cprintf("in dequeue: %s\n", p->name);
  // delete
  if(cnode->next == cnode){
    rq->next_to_run = 0;
  }else{
    struct rq_node* prev = cnode->prev;
    struct rq_node* next = cnode->next;
    prev->next = next;
    next->prev = prev;
    rq->next_to_run = next;
  }

  // free
  cnode->next = rq->free_list;
  rq->free_list = cnode;
}

void yield_proc_simple(struct rq *rq){
  _check_lock(&(rq->rq_lock), "yield_proc_simple no lock");
  if(rq->next_to_run)
    rq->next_to_run = rq->next_to_run->next;
}

struct proc* pick_next_proc_simple(struct rq *rq){
  extern struct proc* idleproc[];//debug
  _check_lock(&(rq->rq_lock), "pick_next_proc_simple no lock");
  if(rq->next_to_run)
    return rq->next_to_run->proc;
  else
    return idleproc[cpu()];
}

void proc_tick_simple(struct rq* rq, struct proc* p){
  _check_lock(&(rq->rq_lock), "proc_tick_simple no lock");
  yield();
}

const struct sched_class simple_sched_class = {
  .enqueue_proc		= enqueue_proc_simple,
  .dequeue_proc		= dequeue_proc_simple,
  .yield_proc		= yield_proc_simple,
  .pick_next_proc	= pick_next_proc_simple,
  .proc_tick		= proc_tick_simple,
};

