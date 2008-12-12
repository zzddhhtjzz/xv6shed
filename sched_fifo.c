#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sched_fifo.h"

void init_rq_fifo(struct rq* rq){
  return;
}

void enqueue_proc_fifo(struct rq *rq, struct proc *p){
  struct rq_node* pnode = rq->free_list;
  rq->free_list = pnode->next;

  // insert after last_to_run
  pnode->proc = p;
  pnode->next = NULL;
  if (rq->next_to_run == NULL){
    rq->next_to_run = rq->last_to_run = pnode;
  } else {
    rq->last_to_run->next = pnode;
    rq->last_to_run = pnode;
  }
}

void dequeue_proc_fifo(struct rq *rq, struct proc *p){
  struct rq_node* node = rq->next_to_run;
  struct rq_node* pnode = NULL;
  while (node != rq->last_to_run && node->proc != p)
  {
    pnode = node;
    node = node->next;
  }
  if (p == rq->next_to_run->proc)
    rq->next_to_run = node->next;
  else
    pnode->next = node->next;
  
  // free
  node->next = rq->free_list;
  rq->free_list = node;
}

void yield_proc_fifo(struct rq *rq){
  if(rq->next_to_run)
  {
    if (rq->next_to_run == rq->last_to_run)
	return;
    rq->last_to_run->next = rq->next_to_run;
    rq->next_to_run = rq->next_to_run->next;
    rq->last_to_run = rq->last_to_run->next;
  }
}

struct proc* pick_next_proc_fifo(struct rq *rq){
  struct rq_node* p_node = rq->next_to_run;
  if (p_node == NULL)
    return NULL;
  else if (p_node->proc->state == RUNNABLE)
    return p_node->proc;
  p_node = p_node->next;
  while (p_node){
    if (p_node->proc->state == RUNNABLE)
      return p_node->proc;
    p_node = p_node->next; 
  }
  return NULL;
}

void proc_tick_fifo(struct rq* rq, struct proc* p){
  extern struct proc* idleproc[];
  if(p == idleproc[cpu()])
    yield();
  return;
}

const struct sched_class sched_class_fifo = {
  .init_rq 		= init_rq_fifo,
  .enqueue_proc		= enqueue_proc_fifo,
  .dequeue_proc		= dequeue_proc_fifo,
  .yield_proc		= yield_proc_fifo,
  .pick_next_proc	= pick_next_proc_fifo,
  .proc_tick		= proc_tick_fifo,
};

