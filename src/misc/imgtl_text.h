#ifndef IMGTL_TEXT_H
#define IMGTL_TEXT_H

int imgtl_memcasecmp(const void *a,const void *b,int c);
int imgtl_decuint_eval(const char *src,int srcc);
int imgtl_utf8_decode(int *dst,const char *src,int srcc);

#endif
