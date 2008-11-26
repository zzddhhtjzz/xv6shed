


#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sched.h"

// by jimmy:
void init_rq_simple(struct rq* rq){
  int i;

  // mark all the nodes is free
  for(i=0; i<NPROC; i++){
    rq->nodes[i].proc = 0;
    rq->nodes[i].next = &(rq->nodes[i]) + sizeof(struct rq_node);
    rq->nodes[i].prev = 0;
  }
  rq->nodes[NPROC-1].next = 0;
  rq->free_list = &(rq->nodes[0]);
  rq->next_to_run = 0;

  // init lock
  initlock(&(rq->rq_lock), "rq_lock");
}

void enqueue_proc_simple(struct rq *rq, struct proc *p){
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
  // search
  struct rq_node* cnode;
  if(rq->next_to_run->proc == p)
    cnode = rq->next_to_run;
  else{
    cnode = rq->next_to_run->prev;
    while(cnode->proc != p){
      if(cnode == rq->next_to_run)
        panic("Kernel panic: Cannot find proc in dequeue_proc_simple\n");
      cnode = cnode->prev;
    }
  }

  // delete
  if(cnode->next == cnode){
    rq->next_to_run = 0;
  }else{
    struct rq_node* prev = cnode->prev;
    struct rq_node* next = cnode->next;
    prev->next = next;
    next->prev = prev;
  }

  // free
  cnode->next = rq->free_list;
  rq->free_list = cnode;
}

void yield_proc_simple(struct rq *rq){
  if(rq->next_to_run)
    rq->next_to_run = rq->next_to_run->next;
  else
    panic("Empty proc yield in yield_proc_simple\n");
}

struct proc* pick_next_proc_simple(struct rq *rq){
  extern struct proc* initproc;//debug
  if(rq->next_to_run)
    return rq->next_to_run->proc;
  else
    return initproc;
}

void proc_tick_simple(struct rq* rq, struct proc* p){
  yield();
}

const struct sched_class simple_sched_class = {
  .enqueue_proc		= enqueue_proc_simple,
  .dequeue_proc		= dequeue_proc_simple,
  .yield_proc		= yield_proc_simple,
  .pick_next_proc	= pick_next_proc_simple,
  .proc_tick		= proc_tick_simple,
};
