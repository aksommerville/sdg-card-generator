#include "imgtl.h"
#include "imgtl_buffer.h"

/* Allocation.
 */
 
void imgtl_buffer_cleanup(struct imgtl_buffer *buffer) {
  if (!buffer) return;
  if (buffer->v) free(buffer->v);
  memset(buffer,0,sizeof(struct imgtl_buffer));
}

int imgtl_buffer_require(struct imgtl_buffer *buffer,int c) {
  if (!buffer) return -1;
  if (c<1) return 0;
  if (buffer->c>INT_MAX-c) return -1;
  int na=buffer->c+c;
  if (na<=buffer->a) return 0;
  if (na<INT_MAX-1024) na=(na+1024)&~1023;
  void *nv=realloc(buffer->v,na);
  if (!nv) return -1;
  buffer->v=nv;
  buffer->a=na;
  return 0;
}

/* Append.
 */

int imgtl_buffer_append(struct imgtl_buffer *buffer,const void *src,int srcc) {
  if ((srcc<0)||(srcc&&!src)) return -1;
  if (imgtl_buffer_require(buffer,srcc)<0) return -1;
  memcpy(buffer->v+buffer->c,src,srcc);
  buffer->c+=srcc;
  return 0;
}

int imgtl_buffer_appendbe(struct imgtl_buffer *buffer,int src,int size) {
  char tmp[4];
  switch (size) {
    case 1: tmp[0]=src; break;
    case 2: tmp[0]=src>>8; tmp[1]=src; break;
    case 3: tmp[0]=src>>16; tmp[1]=src>>8; tmp[2]=src; break;
    case 4: tmp[0]=src>>24; tmp[1]=src>>16; tmp[2]=src>>8; tmp[3]=src; break;
    default: return -1;
  }
  if (imgtl_buffer_require(buffer,size)<0) return -1;
  memcpy(buffer->v+buffer->c,tmp,size);
  buffer->c+=size;
  return 0;
}

int imgtl_buffer_appendle(struct imgtl_buffer *buffer,int src,int size) {
  char tmp[4];
  switch (size) {
    case 1: tmp[0]=src; break;
    case 2: tmp[0]=src; tmp[1]=src>>8; break;
    case 3: tmp[0]=src; tmp[1]=src>>8; tmp[2]=src>>16; break;
    case 4: tmp[0]=src; tmp[1]=src>>8; tmp[2]=src>>16; tmp[3]=src>>24; break;
    default: return -1;
  }
  if (imgtl_buffer_require(buffer,size)<0) return -1;
  memcpy(buffer->v+buffer->c,tmp,size);
  buffer->c+=size;
  return 0;
}
