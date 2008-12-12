#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sched_RR.h"

void init_rq_RR(struct rq* rq){
  int i;

  for(i=0; i<NPROC; i++){
    rq->nodes[i].prev = NULL;
  }
}

void enqueue_proc_RR(struct rq *rq, struct proc *p){
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
  rq->next_to_run = rq->next_to_run->next;
}

struct proc* pick_next_proc_RR(struct rq *rq){
  struct rq_node* p_node = rq->next_to_run;
  if (p_node == NULL)
    return NULL;
  else if (p_node->proc->state == RUNNABLE)
    return p_node->proc;
  p_node = p_node->next;
  while (p_node != rq->next_to_run){
    if (p_node->proc->state == RUNNABLE)
      return p_node->proc;
    p_node = p_node->next; 
  }
  return NULL;
}

void proc_tick_RR(struct rq* rq, struct proc* p){
  //_check_lock(&(rq->rq_lock), "proc_tick_RR no lock");
  p->timeslice--;	// may be some comflict here
  if (p->timeslice == 0)
  {
    p->timeslice = rq->max_slices;
    yield();
  }
}

const struct sched_class sched_class_RR = {
  .init_rq 		= init_rq_RR,
  .enqueue_proc		= enqueue_proc_RR,
  .dequeue_proc		= dequeue_proc_RR,
  .yield_proc		= yield_proc_RR,
  .pick_next_proc	= pick_next_proc_RR,
  .proc_tick		= proc_tick_RR,
};

