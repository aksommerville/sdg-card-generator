#ifndef IMGTL_BUFFER_H
#define IMGTL_BUFFER_H

struct imgtl_buffer {
  char *v;
  int c,a;
};

void imgtl_buffer_cleanup(struct imgtl_buffer *buffer);
int imgtl_buffer_require(struct imgtl_buffer *buffer,int c);

int imgtl_buffer_append(struct imgtl_buffer *buffer,const void *src,int srcc);
int imgtl_buffer_appendbe(struct imgtl_buffer *buffer,int src,int size);
int imgtl_buffer_appendle(struct imgtl_buffer *buffer,int src,int size);

#endif
