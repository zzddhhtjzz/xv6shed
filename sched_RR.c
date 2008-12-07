#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sched_RR.h"

static char* rq_lock_name;

void init_rq_RR(struct rq* rq){
  int i;

  for(i = 0; i < NPROC; i++){
    rq->nodes[i].proc = NULL;
    rq->nodes[i].next = &(rq->nodes[i+1]);
  }
  rq->nodes[NPROC-1].next = NULL;
  rq->free_list = &(rq->nodes[0]);
  rq->next_to_run = NULL;

  // init lock
  initlock(&(rq->rq_lock), rq_lock_name);
}

void enqueue_proc_RR(struct rq *rq, struct proc *p){
//  cprintf("Enqueue: %x, free_list = %x\n", p->pid, rq->free_list);
  if(!holding(&(rq->rq_lock)))
    panic("enqueue_proc_RR no lock");

  // alloc
  if(rq->free_list == NULL)
    panic("kernel panic: Do not support procs more than NPROC!(in enqueue_proc_RR)\n");
  struct rq_node* pnode = rq->free_list;
  rq->free_list = pnode->next;

  // insert before next_to_run
  pnode->proc = p;

  if (rq->next_to_run == NULL){
    rq->next_to_run = rq->last_to_run = pnode;
    pnode->next = pnode->prev = pnode;
  } else {
    rq->next_to_run->prev->next = pnode;
    pnode->prev = rq->next_to_run->prev;
    pnode->next = rq->next_to_run;
    rq->next_to_run->prev = pnode;
  }
}

void dequeue_proc_RR(struct rq *rq, struct proc *p){
  extern struct proc* idleproc[];
  _check_lock(&(rq->rq_lock), "dequeue_proc_RR no lock");

  if(p == idleproc[cpu()])
    return;
  struct rq_node* node = rq->next_to_run;
  while (node != rq->next_to_run->prev && node->proc != p)
  {
    node = node->next;
  }
  if (p == rq->next_to_run->proc)
  {
    if (node->next == node)
      rq->next_to_run = NULL;
    else
      rq->next_to_run = rq->next_to_run->next;
  }
  if (node->next != node)
  {
    node->prev->next = node->next;
    node->next->prev = node->prev;
  }
  
  // free
  node->next = rq->free_list;
  rq->free_list = node;
}

void yield_proc_RR(struct rq *rq){
  _check_lock(&(rq->rq_lock), "yield_proc_RR no lock");
  if(rq->next_to_run)
    rq->next_to_run = rq->next_to_run->next;
}

struct proc* pick_next_proc_RR(struct rq *rq){
  extern struct proc* idleproc[];
  _check_lock(&(rq->rq_lock), "pick_next_proc_RR no lock");
  if(rq->next_to_run)
    return rq->next_to_run->proc;
  else
    return idleproc[cpu()];
}

void proc_tick_RR(struct rq* rq, struct proc* p){
  _check_lock(&(rq->rq_lock), "proc_tick_RR no lock");
  p->timeslice--;
  if (p->timeslice == 0)
  {
    p->timeslice = INIT_SLICE;
    dequeue_proc_RR(rq, p);
    enqueue_proc_RR(rq, p);
  }
  yield();
}

const struct sched_class sched_class_RR = {
  .enqueue_proc		= enqueue_proc_RR,
  .dequeue_proc		= dequeue_proc_RR,
  .yield_proc		= yield_proc_RR,
  .pick_next_proc	= pick_next_proc_RR,
  .proc_tick		= proc_tick_RR,
};

