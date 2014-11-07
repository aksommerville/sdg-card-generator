#ifndef IMGTL_TEXT_H
#define IMGTL_TEXT_H

int imgtl_memcasecmp(const void *a,const void *b,int c);
int imgtl_decuint_eval(const char *src,int srcc);
int imgtl_utf8_decode(int *dst,const char *src,int srcc);
int imgtl_utf8_encode(char *dst,int dsta,int src);
int imgtl_str_measure(const char *src,int srcc);
int imgtl_str_eval(char *dst,int dsta,const char *src,int srcc);

// There is a range of special colors: nnfe0100 where nn is a field ID.
// Note that the alpha of all of those is zero, so they shouldn't cause any conflict.
// We forcibly prevent these special colors from appearing.
int imgtl_rgba_eval(int *dst,const char *src,int srcc);

static inline int imgtl_hexdigit_eval(char src) {
  if ((src>='0')&&(src<='9')) return src-'0';
  if ((src>='a')&&(src<='f')) return src-'a'+10;
  if ((src>='A')&&(src<='F')) return src-'A'+10;
  return -1;
}

#endif
