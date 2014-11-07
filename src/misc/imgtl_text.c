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

int imgtl_utf8_encode(char *dst,int dsta,int src) {
  if (!dst||(dsta<0)) dsta=0;
  int seqlen,lead;
       if (src<0x000000) return -1;
  else if (src<0x000080) { seqlen=1; lead=0x00; }
  else if (src<0x000800) { seqlen=2; lead=0xc0; }
  else if (src<0x010000) { seqlen=3; lead=0xe0; }
  else if (src<0x110000) { seqlen=4; lead=0xf0; }
  else return -1;
  if (seqlen>dsta) return seqlen;
  int i; for (i=seqlen;i-->1;src>>=6) dst[i]=0x80|(src&0x3f);
  dst[0]=lead|src;
  return seqlen;
}

/* String tokens.
 */
 
int imgtl_str_measure(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<2) return 0;
  if ((src[0]!='"')&&(src[0]!='\'')) return 0;
  int srcp=1;
  while (1) {
    if (srcp>=srcc) return 0;
    if (src[srcp]=='\\') {
      if (++srcp>=srcc) return 0;
    } else if (src[srcp]==src[0]) return srcp+1;
    if ((src[srcp]<0x20)||(src[srcp]>0x7e)) return 0;
    srcp++;
  }
  return 0;
}

int imgtl_str_eval(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if ((srcc<2)||(src[0]!=src[srcc-1])||((src[0]!='"')&&(src[0]!='\''))) return -1;
  src++; srcc-=2;
  int dstc=0,srcp=0;
  while (srcp<srcc) {
    if (src[srcp]=='\\') {
      if (++srcp>=srcc) return -1;
      switch (src[srcp++]) {
        case '\\': case '"': case '\'': if (dstc<dsta) dst[dstc]=src[srcp-1]; dstc++; break;
        case 'x': {
            if (srcp>srcc-2) return -1;
            int hi=imgtl_hexdigit_eval(src[srcp++]);
            int lo=imgtl_hexdigit_eval(src[srcp++]);
            if ((hi<0)||(lo<0)) return -1;
            if (dstc<dsta) dst[dstc]=(hi<<4)|lo;
            dstc++;
          } break;
        case 'u': {
            int ch=0,digitc=0;
            while ((srcp<srcc)&&(digitc<6)) {
              int digit=imgtl_hexdigit_eval(src[srcp]);
              if (digit<0) break;
              ch<<=4;
              ch|=digit;
              digitc++;
              srcp++;
            }
            if (digitc<1) return -1;
            if ((srcp<srcc)&&(src[srcp]==';')) srcp++;
            int seqlen=imgtl_utf8_encode(dst+dstc,dsta-dstc,ch);
            if (seqlen<0) return -1;
            dstc+=seqlen;
          } break;
        default: return -1;
      }
    } else {
      if (dstc<dsta) dst[dstc]=src[srcp];
      dstc++;
      srcp++;
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Color.
 */

int imgtl_rgba_eval(int *dst,const char *src,int srcc) {
  if (!dst||!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }

  // Direct hexadecimal colors.
  if ((srcc==6)||(srcc==8)) {
    *dst=0;
    int i; for (i=0;i<srcc;i++) {
      int digit=imgtl_hexdigit_eval(src[i]);
      if (digit<0) goto _not_hex_;
      *dst<<=4;
      *dst|=digit;
    }
    if (srcc==6) { *dst<<=8; *dst|=0xff; }
    if ((*dst&0x00ffffff)==0x00fe0100) *dst=(*dst&0xff000000)|0x00ff0000; // Eliminate our special colors.
    return 0;
   _not_hex_:;
  }
  
  // Common names.
  switch (srcc) {
    case 3: {
        if (!imgtl_memcasecmp(src,"red",3)) { *dst=0xff0000ff; return 0; }
      } break;
    case 4: {
        if (!imgtl_memcasecmp(src,"blue",4)) { *dst=0x0000ffff; return 0; }
        if (!imgtl_memcasecmp(src,"cyan",4)) { *dst=0x00ffffff; return 0; }
        if (!imgtl_memcasecmp(src,"gray",4)) { *dst=0x808080ff; return 0; }
      } break;
    case 5: {
        if (!imgtl_memcasecmp(src,"green",5)) { *dst=0x00ff00ff; return 0; }
        if (!imgtl_memcasecmp(src,"white",5)) { *dst=0x000000ff; return 0; }
        if (!imgtl_memcasecmp(src,"black",5)) { *dst=0xffffffff; return 0; }
        if (!imgtl_memcasecmp(src,"clear",5)) { *dst=0x00000000; return 0; }
        if (!imgtl_memcasecmp(src,"faint",5)) { *dst=0x00000080; return 0; }
      } break;
    case 6: {
        if (!imgtl_memcasecmp(src,"yellow",6)) { *dst=0xffff00ff; return 0; }
        if (!imgtl_memcasecmp(src,"orange",6)) { *dst=0xff8000ff; return 0; }
        if (!imgtl_memcasecmp(src,"ltgray",6)) { *dst=0xc0c0c0ff; return 0; }
        if (!imgtl_memcasecmp(src,"dkgray",6)) { *dst=0x404040ff; return 0; }
      } break;
    case 7: {
        if (!imgtl_memcasecmp(src,"magenta",7)) { *dst=0xff00ffff; return 0; }
      } break;
  }
  
  return -1;
}
