#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sched_fifo.h"

static char* rq_lock_name;

void init_rq_fifo(struct rq* rq){
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

void enqueue_proc_fifo(struct rq *rq, struct proc *p){
//  cprintf("Enqueue: %x, free_list = %x\n", p->pid, rq->free_list);
  if(!holding(&(rq->rq_lock)))
    panic("enqueue_proc_fifo no lock");

  // alloc
  if(rq->free_list == NULL)
    panic("kernel panic: Do not support procs more than NPROC!(in enqueue_proc_fifo)\n");
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
  extern struct proc* idleproc[];
  _check_lock(&(rq->rq_lock), "dequeue_proc_fifo no lock");

  if(p == idleproc[cpu()])
    return;
  struct rq_node* node = rq->next_to_run;
  struct rq_node* pnode = NULL;
  while (node != rq->last_to_run && node->proc != p)
  {
    pnode = node;
    node = node->next;
  }
  if (p == rq->next_to_run)
    rq->next_to_run = node->next;
  else
    pnode->next = node->next;
  
  // free
  node->next = rq->free_list;
  rq->free_list = node;
}

void yield_proc_fifo(struct rq *rq){
  _check_lock(&(rq->rq_lock), "yield_proc_fifo no lock");
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
  extern struct proc* idleproc[];
  _check_lock(&(rq->rq_lock), "pick_next_proc_fifo no lock");
  if(rq->next_to_run)
    return rq->next_to_run->proc;
  else
    return idleproc[cpu()];
}

void proc_tick_fifo(struct rq* rq, struct proc* p){
  _check_lock(&(rq->rq_lock), "proc_tick_fifo no lock");
  yield();
}

const struct sched_class sched_class_fifo = {
  .enqueue_proc		= enqueue_proc_fifo,
  .dequeue_proc		= dequeue_proc_fifo,
  .yield_proc		= yield_proc_fifo,
  .pick_next_proc	= pick_next_proc_fifo,
  .proc_tick		= proc_tick_fifo,
};

