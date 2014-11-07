#include "imgtl.h"
#include "imgtl_png.h"
#include "misc/imgtl_buffer.h"
#include <zlib.h>

#define FAIL(fmt,...) { \
  if (png&&!png->msgc) imgtl_png_set_msgf(png,fmt,##__VA_ARGS__); \
  return -1; \
}

#define RD32(src) ({ \
  const uint8_t *_src=(uint8_t*)(src); \
  (_src[0]<<24)|(_src[1]<<16)|(_src[2]<<8)|_src[3]; \
})

#define WR32(dst,src) { \
  uint8_t *_dst=(uint8_t*)(dst); \
  uint32_t _src=(src); \
  _dst[0]=_src>>24; \
  _dst[1]=_src>>16; \
  _dst[2]=_src>>8; \
  _dst[3]=_src; \
}

#define Z ((z_stream*)png->z)

/* Cleanup.
 */

void imgtl_png_cleanup(struct imgtl_png *png) {
  if (!png) return;
  if (png->pixels) free(png->pixels);
  if (png->plte) free(png->plte);
  if (png->trns) free(png->trns);
  if (png->msg) free(png->msg);
  if (png->z) free(png->z);
  if (png->rowbuf) free(png->rowbuf);
  if (png->chunkv) {
    while (png->chunkc-->0) imgtl_png_chunk_cleanup(png->chunkv+png->chunkc);
    free(png->chunkv);
  }
  memset(png,0,sizeof(struct imgtl_png));
}

void imgtl_png_chunk_cleanup(struct imgtl_png_chunk *chunk) {
  if (!chunk) return;
  if (chunk->v) free(chunk->v);
  memset(chunk,0,sizeof(struct imgtl_png_chunk));
}

/* Message.
 */
 
int imgtl_png_set_msg(struct imgtl_png *png,const char *src,int srcc) {
  if (!png) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { srcc--; src++; }
  if (srcc>255) srcc=255;
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    int i; for (i=0;i<srcc;i++) if ((src[i]<0x20)||(src[i]>0x7e)) nv[i]='?'; else nv[i]=src[i];
    nv[srcc]=0;
  }
  if (png->msg) free(png->msg);
  png->msg=nv;
  png->msgc=srcc;
  return 0;
}

int imgtl_png_set_msgfv(struct imgtl_png *png,const char *fmt,va_list vargs) {
  if (!png) return -1;
  if (!fmt||!fmt[0]) return imgtl_png_set_msg(png,0,0);
  int a=64;
  char *v=malloc(a);
  if (!v) return -1;
  while (1) {
    va_list _vargs;
    va_copy(_vargs,vargs);
    int c=vsnprintf(v,a,fmt,_vargs);
    if ((c<0)||(c>=INT_MAX)) { free(v); return -1; }
    if (c<a) {
      int i; for (i=0;i<c;i++) if ((v[i]<0x20)||(v[i]>0x7e)) v[i]='?';
      if (png->msg) free(png->msg);
      png->msg=v;
      png->msgc=c;
      return 0;
    }
    free(v);
    if (!(v=malloc(a=c+1))) return -1;
  }
}

int imgtl_png_set_msgf(struct imgtl_png *png,const char *fmt,...) {
  va_list vargs;
  va_start(vargs,fmt);
  return imgtl_png_set_msgfv(png,fmt,vargs);
}

/* PLTE and tRNS cache. We just copy whatever the caller provides.
 */
 
int imgtl_png_set_plte(struct imgtl_png *png,const void *src,int srcc) {
  if (!png||(srcc<0)||(srcc&&!src)) return -1;
  void *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc))) return -1;
    memcpy(nv,src,srcc);
  }
  if (png->plte) free(png->plte);
  png->plte=nv;
  png->pltec=srcc;
  return 0;
}

int imgtl_png_set_trns(struct imgtl_png *png,const void *src,int srcc) {
  if (!png||(srcc<0)||(srcc&&!src)) return -1;
  void *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc))) return -1;
    memcpy(nv,src,srcc);
  }
  if (png->trns) free(png->trns);
  png->trns=nv;
  png->trnsc=srcc;
  return 0;
}

int imgtl_png_add_chunk(struct imgtl_png *png,const char *id,const void *src,int srcc) {
  if (!png||!id||(srcc<0)||(srcc&&!src)) return -1;
  if (png->chunkc>=png->chunka) {
    int na=png->chunka+8;
    if (na>INT_MAX/sizeof(struct imgtl_png_chunk)) return -1;
    void *nv=realloc(png->chunkv,sizeof(struct imgtl_png_chunk)*na);
    if (!nv) return -1;
    png->chunkv=nv;
    png->chunka=na;
  }
  struct imgtl_png_chunk *chunk=png->chunkv+png->chunkc++;
  memset(chunk,0,sizeof(struct imgtl_png_chunk));
  if (srcc) {
    if (!(chunk->v=malloc(srcc))) { png->chunkc--; return -1; }
    memcpy(chunk->v,src,srcc);
    chunk->c=srcc;
  }
  memcpy(chunk->id,id,4);
  return 0;
}

/* Unfilter pixels.
 */

static inline void imgtl_png_filter_SUB(uint8_t *dst,const uint8_t *src,int c,int stride) {
  int i=0;
  for (;i<stride;i++) dst[i]=src[i];
  for (;i<c;i++) dst[i]=src[i]+dst[i-stride];
}

static inline void imgtl_png_filter_UP(uint8_t *dst,const uint8_t *src,const uint8_t *pv,int c) {
  int i=0;
  for (;i<c;i++) dst[i]=src[i]+pv[i];
}

static inline void imgtl_png_filter_AVG(uint8_t *dst,const uint8_t *src,const uint8_t *pv,int c,int stride) {
  int i=0;
  if (pv) {
    for (;i<stride;i++) dst[i]=src[i]+(pv[i]>>1);
    for (;i<c;i++) dst[i]=src[i]+((pv[i]+dst[i-stride])>>1);
  } else {
    for (;i<stride;i++) dst[i]=src[i];
    for (;i<c;i++) dst[i]=src[i]+(dst[i-stride]>>1);
  }
}

static inline uint8_t imgtl_PAETH(uint8_t a,uint8_t b,uint8_t c) {
  int p=a+b-c;
  int pa=a-p; if (pa<0) pa=-pa;
  int pb=b-p; if (pb<0) pb=-pb;
  int pc=c-p; if (pc<0) pc=-pc;
  if ((pa<=pb)&&(pa<=pc)) return a;
  if (pb<=pc) return b;
  return c;
}

static inline void imgtl_png_filter_PAETH(uint8_t *dst,const uint8_t *src,const uint8_t *pv,int c,int stride) {
  int i=0;
  if (pv) {
    for (;i<stride;i++) dst[i]=src[i]+pv[i];
    for (;i<c;i++) dst[i]=src[i]+imgtl_PAETH(dst[i-stride],pv[i],pv[i-stride]);
  } else {
    // It's the SUB filter, exactly.
    for (;i<stride;i++) dst[i]=src[i];
    for (;i<c;i++) dst[i]=src[i]+dst[i-stride];
  }
}

/* Receive pixel data in row buffer and prep the next one.
 */

static int imgtl_png_receive_row(struct imgtl_png *png) {
  if ((png->y>=0)&&(png->y<png->h)) {
    uint8_t *dst=(uint8_t*)png->pixels+png->y*png->stride;
    const uint8_t *pv=png->y?(dst-png->stride):0;
    const uint8_t *src=png->rowbuf+1;
    switch (png->rowbuf[0]) {
      case 0x00: memcpy(dst,src,png->stride); break;
      case 0x01: imgtl_png_filter_SUB(dst,src,png->stride,png->xstride); break;
      case 0x02: if (pv) imgtl_png_filter_UP(dst,src,pv,png->stride); else memcpy(dst,src,png->stride); break;
      case 0x03: imgtl_png_filter_AVG(dst,src,pv,png->stride,png->xstride); break;
      case 0x04: imgtl_png_filter_PAETH(dst,src,pv,png->stride,png->xstride); break;
      default: FAIL("Unknown filter 0x%02x for row %d/%d.",png->rowbuf[0],png->y,png->h)
    }
    png->y++;
  }
  Z->next_out=(Bytef*)png->rowbuf;
  Z->avail_out=1+png->stride;
  return 0;
}

/* Decode IDAT.
 */

static int imgtl_png_decode_IDAT(struct imgtl_png *png,const uint8_t *src,int srcc) {
  if (!png->pixels) FAIL("Require IHDR before IDAT.")
  Z->next_in=(Bytef*)src;
  Z->avail_in=srcc;
  while (Z->avail_in) {
    if (!Z->avail_out&&(imgtl_png_receive_row(png)<0)) return -1;
    int err=inflate(Z,Z_NO_FLUSH);
    if (err<0) err=inflate(Z,Z_SYNC_FLUSH);
    if (err<0) FAIL("inflate: zlib error #%d",err)
    if (!Z->avail_out&&(imgtl_png_receive_row(png)<0)) return -1;
  }
  return 0;
}

/* Decode IHDR.
 */

static int imgtl_png_decode_IHDR(struct imgtl_png *png,const uint8_t *src,int srcc) {
  if (png->pixels) FAIL("Multiple IHDR.")
  if (srcc<13) FAIL("Short IHDR.") // XXX Spec says it will always be _exactly_ 13 bytes, but we ignore excess.

  png->w=RD32(src);
  png->h=RD32(src+4);
  png->depth=src[8];
  png->colortype=src[9];
  uint8_t compression=src[10];
  uint8_t filter=src[11];
  uint8_t interlace=src[12];

  if ((png->w<1)||(png->h<1)) FAIL("Invalid dimensions.")
  if (compression) FAIL("Unknown compression type %d.",compression)
  if (filter) FAIL("Unknown filter type %d.",filter)
  if (interlace==1) FAIL("Adam7 interlacing not supported.")
  if (interlace) FAIL("Unknown interlace type %d.",interlace)

  switch (png->colortype) {
    case 0: png->chanc=1; break;
    case 2: png->chanc=3; break;
    case 3: png->chanc=1; break;
    case 4: png->chanc=2; break;
    case 6: png->chanc=4; break;
    default: FAIL("Unknown color type %d.",png->colortype)
  }
  png->pixelsize=png->depth*png->chanc;
  switch (png->pixelsize) {
    case 1: case 2: case 4: case 8: case 16: case 24: case 32: case 40: case 48: case 56: case 64: break;
    default: FAIL("Illegal pixel size %d (depth %d for colortype %d).",png->pixelsize,png->depth,png->colortype)
  }
  if (!(png->xstride=png->pixelsize>>3)) png->xstride=1;

  if (png->w>(INT_MAX-7)/png->pixelsize) FAIL("Image too large.")
  png->stride=(png->w*png->pixelsize+7)>>3;
  if (png->stride>INT_MAX/png->h) FAIL("Image too large.")
  if (!(png->z=calloc(1,sizeof(z_stream)))) FAIL("Out of memory.")
  if (inflateInit(png->z)<0) { free(png->z); png->z=0; FAIL("zlib error.") }
  if (!(png->pixels=malloc(png->stride*png->h))) FAIL("Out of memory.")
  if (png->rowbuf) free(png->rowbuf);
  if (!(png->rowbuf=malloc(1+png->stride))) FAIL("Out of memory.")
  
  png->y=0;
  Z->next_out=(Bytef*)png->rowbuf;
  Z->avail_out=1+png->stride;

  return 0;
}

/* Other chunk -- do we fail, or keep it, or what?
 */

static int imgtl_png_decode_other(struct imgtl_png *png,const char *chunkid,const void *src,int srcc) {

  // If our owner provided a hook, use it.
  if (png->decode_other) {
    if (png->decode_other(png,chunkid,src,srcc)<0) FAIL("User hook failed at %d-byte '%.4s' chunk.",srcc,chunkid)
  }

  // By default, we let unknown chunks go.
  // Owner may ask to check the ANCILLARY bit.
  if (png->flags&IMGTL_PNG_CHECK_ANCILLARY) {
    if (!(chunkid[0]&0x20)) FAIL("Unknown CRITICAL chunk '%.4s'.",chunkid)
  }

  // If requested, cache it.
  if (png->flags&IMGTL_PNG_KEEP_CHUNKS) {
    if (imgtl_png_add_chunk(png,chunkid,src,srcc)<0) return -1;
  }

  return 0;
}

/* Decode, main.
 */

static int imgtl_png_decode_(struct imgtl_png *png,const void *src,int srcc) {
  
  if ((srcc<8)||memcmp(src,"\x89PNG\r\n\x1a\n",8)) FAIL("Signature mismatch.")
  //XXX Spec says IHDR _must_ be first chunk; we do not assert this. Some encoders, eg Apple, occasionally break that rule.
  const uint8_t *SRC=src;
  int srcp=8,term=0;
  while (srcp<=srcc-8) {

    // Generic dechunking.
    uint32_t chunklen=RD32(SRC+srcp);
    const char *chunkid=(char*)SRC+srcp+4;
    srcp+=8;
    if ((chunklen>INT_MAX)||(srcp>srcc-chunklen)) FAIL("Invalid chunk length %u at %#x.",chunklen,srcp-8)
    if (!imgtl_png_chunkid_valid(chunkid)) FAIL("Invalid chunk ID at %#x.",srcp-8)
    const void *chunk=SRC+srcp; srcp+=chunklen;
    if (png->flags&IMGTL_PNG_CHECK_CRC) {
      if (srcp>srcc-4) FAIL("Missing CRC after chunk.")
      uint32_t observed=crc32(crc32(0,0,0),(Bytef*)chunkid,chunklen+4);
      uint32_t stated=RD32(SRC+srcp);
      if (stated!=observed) FAIL("CRC mismatch for '%.4s' chunk at %#x.",chunkid,srcp-chunklen-8)
    }
    srcp+=4;

    // Dispatch based on chunk ID.
    if (!memcmp(chunkid,"IEND",4)) { term=1; break; }
    else if (!memcmp(chunkid,"IHDR",4)) { if (imgtl_png_decode_IHDR(png,chunk,chunklen)<0) return -1; }
    else if (!memcmp(chunkid,"IDAT",4)) { if (imgtl_png_decode_IDAT(png,chunk,chunklen)<0) return -1; }
    else if (!memcmp(chunkid,"PLTE",4)) { if (imgtl_png_set_plte(png,chunk,chunklen)<0) return -1; }
    else if (!memcmp(chunkid,"tRNS",4)) { if (imgtl_png_set_trns(png,chunk,chunklen)<0) return -1; }
    else if (imgtl_png_decode_other(png,chunkid,chunk,chunklen)<0) return -1;
    
  }

  // Wrap it up.
  if (!png->pixels) FAIL("Missing image header.")
  if (!png->y) FAIL("Missing image data.")
  if (png->y<png->h) FAIL("Image data incomplete (%d of %d rows).",png->y,png->h)
  if (png->flags&IMGTL_PNG_VALIDATE_PLTE) {
    if ((png->pltec%3)||(png->pltec>768)) FAIL("Unexpected length %d for PLTE.",png->pltec)
    if ((png->colortype==3)&&!png->pltec) FAIL("PLTE required for indexed pixels.")
  }
  if (!term&&(png->flags&IMGTL_PNG_REQUIRE_IEND)) FAIL("Expected IEND chunk.")
  
  return 0;
}
 
int imgtl_png_decode(struct imgtl_png *png,const void *src,int srcc) {

  if (!png||(srcc<0)||!src) return -1;
  if (png->z) FAIL("Dirty context.")
  if (png->pixels) { free(png->pixels); png->pixels=0; }

  // If it fails, we must clean up the Z context. (Otherwise, we don't know whether it's inflateEnd() or deflateEnd()).
  // We might as well free the pixels too, in case our caller forgets.
  int err=imgtl_png_decode_(png,src,srcc);
  if (png->z) { inflateEnd(png->z); free(png->z); png->z=0; }
  if (err<0) {
    if (png->pixels) { free(png->pixels); png->pixels=0; }
    return -1;
  }

  return 0;
}

/* Validate chunk ID.
 */
 
int imgtl_png_chunkid_valid(const char *src) {
  if (!src) return 0;
  if ((src[0]<0x20)||(src[0]>0x7e)) return 0;
  if ((src[1]<0x20)||(src[1]>0x7e)) return 0;
  if ((src[2]<0x20)||(src[2]>0x7e)) return 0;
  if ((src[3]<0x20)||(src[3]>0x7e)) return 0;
  return 1;
}

/* Apply filter to single row.
 */

static void imgtl_png_filter_row(uint8_t *dst,uint8_t filter,const uint8_t *src,const uint8_t *pv,int c,int stride) {
  int i=0;
  *dst++=filter;
  switch (filter) {
    case 1: {
        for (;i<stride;i++) dst[i]=src[i];
        for (;i<c;i++) dst[i]=src[i]-src[i-stride];
      } break;
    case 2: if (pv) {
        for (;i<c;i++) dst[i]=src[i]-pv[i];
      } else {
        memcpy(dst,src,c);
      } break;
    case 3: if (pv) {
        for (;i<stride;i++) dst[i]=src[i]-(pv[i]>>1);
        for (;i<c;i++) dst[i]=src[i]-((pv[i]+src[i-stride])>>1);
      } else {
        for (;i<stride;i++) dst[i]=src[i];
        for (;i<c;i++) dst[i]=src[i]-(src[i-stride]>>1);
      } break;
    case 4: if (pv) {
        for (;i<stride;i++) dst[i]=src[i]-pv[i];
        for (;i<c;i++) dst[i]=src[i]-imgtl_PAETH(src[i-stride],pv[i],pv[i-stride]);
      } else {
        for (;i<stride;i++) dst[i]=src[i];
        for (;i<c;i++) dst[i]=src[i]-src[i-stride];
      } break;
    case 0: default: memcpy(dst,src,c);
  }
}

/* Compress one row of pixels.
 */

static void *imgtl_png_write_filtered_row(struct imgtl_png *png,int y) {
  switch (png->filter_strategy) {
  
    case IMGTL_PNG_FILTER_LONGEST_RUN: {
        int stride=1+png->stride;
        uint8_t *frow=png->rowbuf;
        const uint8_t *src=(uint8_t*)png->pixels+y*png->stride;
        const uint8_t *pv=y?(src-png->stride):0;
        int hiscore=0,best=0;
        int i; for (i=0;i<5;i++,frow+=stride) {
          imgtl_png_filter_row(frow,i,src,pv,png->stride,png->xstride);
          int score=0,j; 
          for (j=1;j<=png->stride;) {
            int runc=1; while ((j+runc<png->stride)&&(frow[j]==frow[j+runc])) runc++;
            j+=runc;
            if (runc>score) score=runc;
          }
          if (score>hiscore) { hiscore=score; best=i; }
        }
        return (char*)png->rowbuf+stride*best;
      }
      
    case IMGTL_PNG_FILTER_MOST_NEIGHBORS: {
        int stride=1+png->stride;
        uint8_t *frow=png->rowbuf;
        const uint8_t *src=(uint8_t*)png->pixels+y*png->stride;
        const uint8_t *pv=y?(src-png->stride):0;
        int hiscore=0,best=0;
        int i; for (i=0;i<5;i++,frow+=stride) {
          imgtl_png_filter_row(frow,i,src,pv,png->stride,png->xstride);
          int score=0,j; for (j=1;j<png->stride;j++) if (frow[j]==frow[j+1]) score++;
          if (score>hiscore) { hiscore=score; best=i; }
        }
        return (char*)png->rowbuf+stride*best;
      }
      
    case IMGTL_PNG_FILTER_ALWAYS_NONE:
    case IMGTL_PNG_FILTER_ALWAYS_SUB:
    case IMGTL_PNG_FILTER_ALWAYS_UP:
    case IMGTL_PNG_FILTER_ALWAYS_AVG:
    case IMGTL_PNG_FILTER_ALWAYS_PAETH: {
        const uint8_t *src=(uint8_t*)png->pixels+y*png->stride;
        const uint8_t *pv=y?(src-png->stride):0;
        imgtl_png_filter_row(png->rowbuf,png->filter_strategy-IMGTL_PNG_FILTER_ALWAYS_NONE,src,pv,png->stride,png->xstride);
        Z->next_in=(Bytef*)png->rowbuf;
        Z->avail_in=1+png->stride;
        return png->rowbuf;
      }
      
    case IMGTL_PNG_FILTER_MOST_ZEROES: default: {
        int stride=1+png->stride;
        uint8_t *frow=png->rowbuf;
        const uint8_t *src=(uint8_t*)png->pixels+y*png->stride;
        const uint8_t *pv=y?(src-png->stride):0;
        int hiscore=0,best=0;
        int i; for (i=0;i<5;i++,frow+=stride) {
          imgtl_png_filter_row(frow,i,src,pv,png->stride,png->xstride);
          int score=0,j; for (j=1;j<=png->stride;j++) if (!frow[j]) score++;
          if (score>hiscore) { hiscore=score; best=i; }
        }
        return (char*)png->rowbuf+stride*best;
      }
      
  }
  return 0;
}

/* Filter and compress single row.
 */

static int imgtl_png_encode_row(struct imgtl_buffer *dst,struct imgtl_png *png,int y) {
  if (!(Z->next_in=(Bytef*)imgtl_png_write_filtered_row(png,y))) return -1;
  Z->avail_in=1+png->stride;
  while (Z->avail_in>0) {
    if (imgtl_buffer_require(dst,1)<0) return -1;
    Z->next_out=(Bytef*)dst->v+dst->c;
    Z->avail_out=dst->a-dst->c;
    int ao0=Z->avail_out;
    int err=deflate(Z,Z_NO_FLUSH);
    if (err<0) err=deflate(Z,Z_SYNC_FLUSH);
    if (err<0) FAIL("deflate: zlib error %d",err)
    int written=ao0-Z->avail_out;
    dst->c+=written;
  }
  return 0;
}

/* Compress entire image at once.
 * If this fails, we'll try going row-by-row.
 * ...Turns out, this is not the performance boost I expected.
 */

static int imgtl_png_encode_idat(struct imgtl_buffer *dst,struct imgtl_png *png) {

  int fstride=1+png->stride;
  int fsize=fstride*png->h;
  uint8_t *fpixels=malloc(fsize);
  if (!fpixels) return -1;
  uint8_t *fdst=fpixels;
  int y; for (y=0;y<png->h;y++) {
    const void *row=imgtl_png_write_filtered_row(png,y);
    if (!row) { free(fpixels); return -1; }
    memcpy(fdst,row,fstride);
    fdst+=fstride;
  }
  
  Z->next_in=(Bytef*)fpixels;
  Z->avail_in=fsize;
  while (Z->avail_in>0) {
    if (imgtl_buffer_require(dst,1)<0) { free(fpixels); return -1; }
    Z->next_out=(Bytef*)dst->v+dst->c;
    Z->avail_out=dst->a-dst->c;
    int ao0=Z->avail_out;
    int err=deflate(Z,Z_NO_FLUSH);
    if (err<0) err=deflate(Z,Z_SYNC_FLUSH);
    if (err<0) { free(fpixels); return -1; }
    int written=ao0-Z->avail_out;
    dst->c+=written;
  }

  free(fpixels);
  return 0;
}

/* Flush compressor into buffer.
 */

static int imgtl_png_encode_flush_z(struct imgtl_buffer *dst,struct imgtl_png *png) {
  Z->next_in=0;
  Z->avail_in=0;
  while (1) {
    if (imgtl_buffer_require(dst,1)<0) return -1;
    Z->next_out=(Bytef*)dst->v+dst->c;
    Z->avail_out=dst->a-dst->c;
    int ao0=Z->avail_out;
    int err=deflate(Z,Z_FINISH);
    if (err<0) FAIL("deflate: zlib error %d",err)
    int written=ao0-Z->avail_out;
    dst->c+=written;
    if (err==Z_STREAM_END) break;
  }
  return 0;
}

/* Encode single chunk, generating wrapper.
 */

static int imgtl_png_encode_chunk(struct imgtl_buffer *dst,const char *id,const void *src,int srcc) {
  if (imgtl_buffer_appendbe(dst,srcc,4)<0) return -1;
  int crcbeginp=dst->c;
  if (imgtl_buffer_append(dst,id,4)<0) return -1;
  if (imgtl_buffer_append(dst,src,srcc)<0) return -1;
  uint32_t crc=crc32(crc32(0,0,0),(Bytef*)dst->v+crcbeginp,dst->c-crcbeginp);
  if (imgtl_buffer_appendbe(dst,crc,4)<0) return -1;
  return 0;
}

/* Encode. Initial checks cleared.
 */

static int imgtl_png_encode_(struct imgtl_buffer *dst,struct imgtl_png *png) {
  int i;

  // Signature and IHDR.
  { char buf[33];
    memcpy(buf,"\x89PNG\r\n\x1a\n\0\0\0\xdIHDR",16);
    WR32(buf+16,png->w)
    WR32(buf+20,png->h)
    buf[24]=png->depth;
    buf[25]=png->colortype;
    buf[26]=0; // compression
    buf[27]=0; // filter
    buf[28]=0; // interlace
    uint32_t crc=crc32(crc32(0,0,0),(Bytef*)buf+12,17);
    WR32(buf+29,crc)
    if (imgtl_buffer_append(dst,buf,33)<0) return -1;
  }

  // PLTE, tRNS, and loose chunks.
  if ((png->pltec>0)&&(imgtl_png_encode_chunk(dst,"PLTE",png->plte,png->pltec)<0)) return -1;
  if ((png->trnsc>0)&&(imgtl_png_encode_chunk(dst,"tRNS",png->trns,png->trnsc)<0)) return -1;
  for (i=0;i<png->chunkc;i++) {
    if (imgtl_png_encode_chunk(dst,png->chunkv[i].id,png->chunkv[i].v,png->chunkv[i].c)<0) return -1;
  }

  // IDAT.
  { int lenp=dst->c;
    if (imgtl_buffer_append(dst,"\0\0\0\0IDAT",8)<0) return -1;
    if (imgtl_png_encode_idat(dst,png)<0) {
      for (i=0;i<png->h;i++) {
        if (imgtl_png_encode_row(dst,png,i)<0) return -1;
      }
    }
    if (imgtl_png_encode_flush_z(dst,png)<0) return -1;
    int len=dst->c-lenp-8;
    WR32(dst->v+lenp,len)
    uint32_t crc=crc32(crc32(0,0,0),(Bytef*)dst->v+lenp+4,len+4);
    if (imgtl_buffer_appendbe(dst,crc,4)<0) return -1;
  }

  // IEND.
  if (imgtl_buffer_append(dst,"\0\0\0\0IEND\xae\x42\x60\x82",12)<0) return -1;

  return 0;
}

/* Encode, main entry point.
 */

int imgtl_png_encode(struct imgtl_buffer *dst,struct imgtl_png *png) {

  // Validate and fill in header.
  if (!dst||!png) return -1;
  if (!png->pixels) FAIL("Invalid image.")
  if (png->z) return -1;
  if ((png->w<1)||(png->h<1)) FAIL("Invalid image.")
  switch (png->colortype) {
    case 0: png->chanc=1; break;
    case 2: png->chanc=3; break;
    case 3: png->chanc=1; break;
    case 4: png->chanc=2; break;
    case 6: png->chanc=4; break;
    default: FAIL("Invalid format.")
  }
  png->pixelsize=png->depth*png->chanc;
  if (png->flags&IMGTL_PNG_LOOSE_FORMAT) { // Confirm total is factor or multiple of 8.
    switch (png->pixelsize) {
      case 1: case 2: case 4: case 8: case 16: case 24: case 32: case 40: case 48: case 56: case 64: break;
      default: FAIL("Invalid format.")
    }
  } else { // Confirm format is explicitly permitted by spec.
    switch (png->colortype) {
      case 0: switch (png->depth) { case 1: case 2: case 4: case 8: case 16: break; default: FAIL("Invalid format.") } break;
      case 2: switch (png->depth) { case 8: case 16: break; default: FAIL("Invalid format.") } break;
      case 3: switch (png->depth) { case 1: case 2: case 4: case 8: break; default: FAIL("Invalid format.") } break;
      case 4: switch (png->depth) { case 8: case 16: break; default: FAIL("Invalid format.") } break;
      case 6: switch (png->depth) { case 8: case 16: break; default: FAIL("Invalid format.") } break;
    }
  }
  if (!(png->xstride=png->pixelsize>>3)) png->xstride=1;
  if (png->w>(INT_MAX-7)/png->pixelsize) FAIL("Image too large.")
  png->stride=(png->w*png->pixelsize+7)>>3;
  if (png->stride<1) FAIL("Image too large.")

  // Prep compressor and row buffer.
  int rowbufc=1+png->stride;
  if (rowbufc>INT_MAX/5) return -1;
  rowbufc*=5;
  if (png->rowbuf) free(png->rowbuf);
  if (!(png->rowbuf=malloc(rowbufc))) return -1;
  if (!(png->z=calloc(1,sizeof(z_stream)))) return -1;
  int cmplevel=(png->flags&IMGTL_PNG_UNCOMPRESSED)?0:Z_BEST_COMPRESSION;
  if (deflateInit(png->z,cmplevel)<0) { free(png->z); png->z=0; return -1; }

  // Enter inner function -- ensure that png->z gets cleaned up, error or no.
  int err=imgtl_png_encode_(dst,png);
  deflateEnd(png->z);
  free(png->z);
  png->z=0;
  return err;
}

/* Fixed-format encoding convenience.
 */

int imgtl_png_encode_simple(struct imgtl_buffer *dst,const void *src,int w,int h,int depth,int colortype) {
  struct imgtl_png png={0};
  png.pixels=(void*)src;
  png.w=w;
  png.h=h;
  png.depth=depth;
  png.colortype=colortype;
  int err=imgtl_png_encode(dst,&png);
  png.pixels=0;
  imgtl_png_cleanup(&png);
  return err;
}
