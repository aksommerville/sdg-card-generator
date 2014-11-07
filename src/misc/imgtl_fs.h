#ifndef IMGTL_FS_H
#define IMGTL_FS_H

int imgtl_file_read(void *dstpp,const char *path);
int imgtl_file_write(const char *path,const void *src,int srcc);

#endif
