// by jimmy:

void init_rq_simple(struct rq*);

struct sched_class{
  // put the proc into runqueue
  void (*enqueue_proc) (struct rq *rq, struct proc *p);
  // get the proc out runqueue
  void (*dequeue_proc) (struct rq *rq, struct proc *p);
  //yield
  void (*yield_proc) (struct rq *rq);
  // choose the next runnable task
  struct proc* (*pick_next_proc) (struct rq *rq);
  // dealer of the time-tick
  void (*proc_tick)(struct rq* rq, struct proc* p);
};

extern const struct sched_class simple_sched_class;

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
};