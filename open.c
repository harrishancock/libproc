#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  pid_t pid;
} subprocess_t;

void launch_child_process (const char* filename, char* const* argv, int fdpipe) {
  /* Read all open file descriptors with our parent's help. We cannot do this
   * in the child's forked process, because this code could be called from a
   * multithreaded application or library. In that case, the code in between a
   * fork and an execve must be async-safe: i.e., only the functions listed as
   * such in signal(7) may be used. */
  ssize_t nread;
  int fd;
  while ((nread = read(fdpipe, &fd, sizeof(fd)))) {
    if (-1 == nread || nread != sizeof(fd)) {
      write(STDERR_FILENO, "fuck\n", sizeof("fuck\n"));
    }
    else if (2 < fd && fdpipe != fd) {
      if (-1 == close(fd)) {
        write(STDERR_FILENO, "fuck2\n", sizeof("fuck2\n"));
      }
    }
  }

  /* One last file descriptor to close. */
  if (-1 == close(fdpipe)) {
    write(STDERR_FILENO, "fuck4\n", sizeof("fuck4\n"));
  }

  /* Execute the new process. */
  if (-1 == execve(filename, argv, NULL)) {
    write(STDERR_FILENO, "fuck3\n", sizeof("fuck3"));
  }
}

DIR* open_proc_fd (pid_t pid) {
  /* Construct the /proc/<pid>/fd path. */
  char buf[64];
  if (sizeof(buf) <= snprintf(buf, sizeof(buf), "/proc/%d/fd", pid)) {
    /* If we overflowed our 64-byte buffer, there's probably something
     * seriously wrong with pid. */
    errno = EINVAL;
    return NULL;
  }

  /* Caller can deal with any error handling. */
  return opendir(buf);
}

int feed_child_process (pid_t pid, int fdpipe) {
  int ret = 0;

  /* Open the child process' /proc file descriptor directory. */
  DIR* fddir = open_proc_fd(pid);
  if (!fddir) {
    perror("open_proc_fd");
    return -1;
  }

  /* Inform the child of its every open file descriptor (every numeric entry
   * in fddir). */
  int rc;
  struct dirent entry;
  struct dirent* result;
  while (!(rc = readdir_r(fddir, &entry, &result)) && result) {
    if (isdigit(entry.d_name[0])) {
      int fd = atoi(entry.d_name);
      if (-1 == write(fdpipe, &fd, sizeof(fd))) {
        perror("write");
        ret = -1;
      }
    }
  }

  if (rc) {
    fprintf(stderr, "readdir_r: %s\n", strerror(rc));
    ret = -1;
  }

  if (-1 == closedir(fddir)) {
    perror("closedir");
    ret = -1;
  }

  return ret;
}


subprocess_t open_subprocess (const char* filename, char* const* argv) {
  subprocess_t proc = {
    .pid = -1,
  };

  /* pipe(2) initializes a 2-element array with a read file descriptor in
   * index 0, and a write file descriptor in index 1. */
  enum { READ, WRITE };

  /* We create a pipe so we can later send a list of open file descriptors
   * that the child needs to close. */
  int fdpipe[2];
  if (-1 == pipe(fdpipe)) {
    perror("pipe");
    return proc;
  }

  proc.pid = fork();
  if (-1 == proc.pid) {
    return proc;
  }
  else if (0 == proc.pid) {
    launch_child_process(filename, argv, fdpipe[READ]);
    abort();  // never reached
  }

  if (-1 == feed_child_process(proc.pid, fdpipe[WRITE])) {
    /* Do something. Do we care? */
  }

  if (-1 == close(fdpipe[WRITE]) ||
      -1 == close(fdpipe[READ])) {
    /* Do something. Do we care? */
  }

  return proc;
}

int close_subprocess (subprocess_t proc) {
  int status;
  waitpid(proc.pid, &status, 0);
  return WEXITSTATUS(status);
}

int main () {
  char* myargv[] = {
    strdup("/bin/sh"),
    strdup("-c"),
    strdup("ls"),
    NULL
  };

  subprocess_t proc = open_subprocess("/bin/sh", myargv);
  close_subprocess(proc);

  for (int i = 0; i < sizeof(myargv) / sizeof(myargv[0]); ++i) {
    free(myargv[i]);
  }
}
