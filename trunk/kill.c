#include "types.h"
#include "stat.h"
#include "user.h"
// just for test of svn, add some commment
// just for test of svn, add some commment
int
main(int argc, char **argv)
{
  int i;
	
  if(argc < 1){
    printf(2, "usage: kill pid...\n");
    exit();
  }
  for(i=1; i<argc; i++)
    kill(atoi(argv[i]));
  exit();
}