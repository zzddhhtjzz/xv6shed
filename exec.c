#include "types.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"
#include "spinlock.h"
#include "sched.h"

#define _check_curproc(a) do{	\
  if(cp == 0){		\
    cprintf("%d, cp == 0\n", a);	\
    panic("");\
  }\
}while(0)\

int
exec(char *path, char **argv)
{
  //_check_curproc(1);
  //acquire(&proc_table_lock);
  //acquire(&(cp->rq->rq_lock));

  //debug
  //cprintf("exec: cpu%d, [%s]\n", cpu(), cp->name);

  char *mem, *s, *last;
  int i, argc, arglen, len, off;
  uint sz, sp, argp;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;

  //cprintf("#0\n", cp->name);
  if((ip = namei(path)) == 0)
    return -1;
  //cprintf("#0.5\n", cp->name);
  ilock(ip);

  // Compute memory size of new process.
  mem = 0;
  sz = 0;

  // Program segments.
  //cprintf("#1\n", cp->name);
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) < sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    sz += ph.memsz;
  }
  
  // Arguments.
  //cprintf("#2\n", cp->name);
  arglen = 0;
  for(argc=0; argv[argc]; argc++)
    arglen += strlen(argv[argc]) + 1;
  arglen = (arglen+3) & ~3;
  sz += arglen + 4*(argc+1);

  // Stack.
  sz += PAGE;
  
  // Allocate program memory.
  //cprintf("#3\n", cp->name);
  sz = (sz+PAGE-1) & ~(PAGE-1);
  mem = kalloc(sz);
  if(mem == 0)
    goto bad;
  memset(mem, 0, sz);

  // Load program into memory.
  //cprintf("#4\n", cp->name);
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.va + ph.memsz > sz)
      goto bad;
    if(readi(ip, mem + ph.va, ph.offset, ph.filesz) != ph.filesz)
      goto bad;
    memset(mem + ph.va + ph.filesz, 0, ph.memsz - ph.filesz);
  }
  iunlockput(ip);
  
  // Initialize stack.
  sp = sz;
  argp = sz - arglen - 4*(argc+1);

  // Copy argv strings and pointers to stack.
  //cprintf("#5\n", cp->name);
  *(uint*)(mem+argp + 4*argc) = 0;  // argv[argc]
  for(i=argc-1; i>=0; i--){
    len = strlen(argv[i]) + 1;
    sp -= len;
    memmove(mem+sp, argv[i], len);
    *(uint*)(mem+argp + 4*i) = sp;  // argv[i]
  }

  // Stack frame for main(argc, argv), below arguments.
  sp = argp;
  sp -= 4;
  *(uint*)(mem+sp) = argp;
  sp -= 4;
  *(uint*)(mem+sp) = argc;
  sp -= 4;
  *(uint*)(mem+sp) = 0xffffffff;   // fake return pc

  // Save program name for debugging.
  //cprintf("#6\n", cp->name);
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(cp->name, last, sizeof(cp->name));

  // Commit to the new image.
  //cprintf("#7\n", cp->name);
  kfree(cp->mem, cp->sz);
  cp->mem = mem;
  cp->sz = sz;
  cp->tf->eip = elf.entry;  // main
  cp->tf->esp = sp;
  setupsegs(cp);

  //debug
  //cprintf("exec end: [%s]\n", cp->name);

  //_check_curproc(2);
  //release(&(cp->rq->rq_lock));
  //release(&proc_table_lock);

  return 0;

 bad:
  if(mem)
    kfree(mem, sz);
  iunlockput(ip);

  //_check_curproc(3);
  //release(&(cp->rq->rq_lock));
  //release(&proc_table_lock);

  return -1;
}
