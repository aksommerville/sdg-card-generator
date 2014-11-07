#include "imgtl.h"
#include "imgtl_fs.h"
#include <unistd.h>
#include <fcntl.h>

/* Read whole file.
 */

int imgtl_file_read(void *dstpp,const char *path) {
  if (!dstpp||!path||!path[0]) return -1;
  int fd=open(path,O_RDONLY);
  if (fd<0) return -1;
  off_t flen=lseek(fd,0,SEEK_END);
  if ((flen<0)||(flen>INT_MAX)||lseek(fd,0,SEEK_SET)) { close(fd); return -1; }
  char *dst=malloc(flen);
  if (!dst) { close(fd); return -1; }
  int dstc=0;
  while (dstc<flen) {
    int err=read(fd,dst+dstc,flen-dstc);
    if (err<=0) { close(fd); free(dst); return -1; }
    dstc+=err;
  }
  close(fd);
  *(void**)dstpp=dst;
  return dstc;
}

/* Write whole file.
 */

int imgtl_file_write(const char *path,const void *src,int srcc) {
  if (!path||!path[0]||(srcc<0)||(srcc&&!src)) return -1;
  int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_APPEND,0666);
  if (fd<0) return -1;
  int srcp=0,err;
  while (srcp<srcc) {
    if ((err=write(fd,(char*)src+srcp,srcc-srcp))<=0) { close(fd); unlink(path); return -1; }
    srcp+=err;
  }
  close(fd);
  return 0;
}
