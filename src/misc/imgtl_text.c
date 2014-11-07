#include "imgtl.h"
#include "imgtl_text.h"

/* memcasecmp.
 */

int imgtl_memcasecmp(const void *a,const void *b,int c) {
  if (a==b) return 0;
  if (c<1) return 0;
  if (!a) return -1;
  if (!b) return 1;
  while (c--) {
    uint8_t cha=*(uint8_t*)a; a=(char*)a+1; if ((cha>=0x41)&&(cha<=0x5a)) cha+=0x20;
    uint8_t chb=*(uint8_t*)b; b=(char*)b+1; if ((chb>=0x41)&&(chb<=0x5a)) chb+=0x20;
    if (cha<chb) return -1;
    if (cha>chb) return 1;
  }
  return 0;
}

/* Evaluate integer.
 */

int imgtl_decuint_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return -1;
  int dst=0,srcp=0;
  while (srcp<srcc) {
    int digit=src[srcp++]-'0';
    if ((digit<0)||(digit>9)) return -1;
    if (dst>INT_MAX/10) return -1; dst*=10;
    if (dst>INT_MAX-digit) return -1; dst+=digit;
  }
  return dst;
}

/* Decode UTF-8.
 */
 
int imgtl_utf8_decode(int *dst,const char *src,int srcc) {
  if (!dst||!src) return -1;
  if (srcc<1) return -1;
  *dst=*(unsigned char*)src;
  if (!(*dst&0x80)) return 1;
  if (!(*dst&0x40)) return -1;
  int seqlen;
  if (!(*dst&0x20)) { seqlen=2; *dst&=0x1f; }
  else if (!(*dst&0x10)) { seqlen=3; *dst&=0x0f; }
  else if (!(*dst&0x08)) { seqlen=4; *dst&=0x07; }
  else return -1;
  if (seqlen>srcc) return -1;
  int i; for (i=1;i<seqlen;i++) {
    if ((src[i]&0xc0)!=0x80) return -1;
    *dst<<=6;
    *dst|=src[i]&0x3f;
  }
  return seqlen;
}
