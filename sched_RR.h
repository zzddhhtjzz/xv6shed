// by cxyzs7:

#ifndef __SCHED_RR_H
#define __SCHED_RR_H

#include "sched.h"

#define INIT_SLICE 3

void init_rq_RR(struct rq*);

extern const struct sched_class sched_class_RR;

/*
struct rq_node{
  struct proc* proc;	// real data
  struct rq_node* next; // double link list, if this rq_node is free,
  			// next will point to the next free rq_node
  struct rq_node* prev;	
};

struct rq{
  struct spinlock rq_lock;
  struct rq_node nodes[NPROC];  // rq_node table
  
  struct rq_node* next_to_run;	// next proc to run
  struct rq_node* free_list;	// all the free proc
};*/
#endif

