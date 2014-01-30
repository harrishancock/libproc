#include "libproc.h"
#include "arraylen.h"

int main () {
  const char* procargv[] = {
    "/bin/sh", "-c", "ls"
  };

  const char* procenvp[] = { };

  int dup2s[] = {
    inpipe[READ], STDIN_FILENO,
    outpipe[WRITE], STDOUT_FILENO,
    errpipe[WRITE], STDERR_FILENO
  };

  int flags = PROC_FORCE_CLOEXEC;

  proc_t proc = proc_open(procargv[0],
      procargv, ARRAYLEN(procargv),
      procenvp, ARRAYLEN(procenvp),
      dup2s, ARRAYLEN(dup2s),
      flags);

  proc_close(proc);
}
