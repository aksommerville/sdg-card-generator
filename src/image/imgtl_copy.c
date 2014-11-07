#include "imgtl.h"
#include "imgtl_image.h"

/* Copy pixels, main entry point.
 */
 
int imgtl_copy(struct imgtl_image *dst,int dstx,int dsty,const struct imgtl_image *src,int srcx,int srcy,int w,int h) {

  // Clamp boundaries.
  if (!dst||!src) return -1;
  if (!dst->write_pixels) return -1;
  if ((w<1)||(h<1)) return 0;
  if (dstx<0) { if ((w+=dstx)<1) return 0; srcx-=dstx; dstx=0; }
  if (dsty<0) { if ((h+=dsty)<1) return 0; srcy-=dsty; dsty=0; }
  if (srcx<0) { if ((w+=srcx)<1) return 0; dstx-=srcx; srcx=0; }
  if (srcy<0) { if ((h+=srcy)<1) return 0; dsty-=srcy; srcy=0; }
  if (dstx>dst->w-w) { if ((w=dst->w-dstx)<1) return 0; }
  if (dsty>dst->h-h) { if ((h=dst->h-dsty)<1) return 0; }
  if (srcx>src->w-w) { if ((w=src->w-srcx)<1) return 0; }
  if (srcy>src->h-h) { if ((h=src->h-srcy)<1) return 0; }

  // Get row pointers.
  uint8_t *dstrow=(uint8_t*)dst->pixels+dsty*dst->stride+dstx*dst->pixelsize;
  const uint8_t *srcrow=(uint8_t*)src->pixels+srcy*src->stride+srcx*src->pixelsize;

  // If they are the same format, we can simply memcpy() each row.
  if (dst->fmt==src->fmt) {
    int cpc=dst->pixelsize*w;
    while (h-->0) {
      memcpy(dstrow,srcrow,cpc);
      dstrow+=dst->stride;
      srcrow+=src->stride;
    }
    return 0;
  }

  // --- Insert other optimized copies here. ---
  //TODO It would be wise to optimize for common grayscale formats. Those are the most expensive pixel accessors.

  // Copy each pixel individually.
  imgtl_rgba_from_pixel_fn rdpx=imgtl_fmt_get_reader(src->fmt);
  imgtl_pixel_from_rgba_fn wrpx=imgtl_fmt_get_writer(dst->fmt);
  if (!rdpx||!wrpx) return -1;
  int rverbatim=imgtl_pixel_reader_is_verbatim(rdpx);
  int wverbatim=imgtl_pixel_writer_is_verbatim(wrpx);
  if (rverbatim&&wverbatim) { // Nevermind the accessor functions, we can copy right from one to the other.
    while (h-->0) {
      uint8_t *dsti=dstrow; dstrow+=dst->stride;
      const uint8_t *srci=srcrow; srcrow+=src->stride;
      int i; for (i=0;i<w;i++) {
        memcpy(dsti,srci,4);
        dsti+=dst->pixelsize;
        srci+=src->pixelsize;
      }
    }
  } else if (rverbatim) {
    while (h-->0) {
      uint8_t *dsti=dstrow; dstrow+=dst->stride;
      const uint8_t *srci=srcrow; srcrow+=src->stride;
      uint8_t rgba[4];
      int i; for (i=0;i<w;i++) {
        wrpx(dsti,srci);
        dsti+=dst->pixelsize;
        srci+=src->pixelsize;
      }
    }
  } else if (wverbatim) {
    while (h-->0) {
      uint8_t *dsti=dstrow; dstrow+=dst->stride;
      const uint8_t *srci=srcrow; srcrow+=src->stride;
      uint8_t rgba[4];
      int i; for (i=0;i<w;i++) {
        rdpx(dsti,srci);
        dsti+=dst->pixelsize;
        srci+=src->pixelsize;
      }
    }
  } else { // General case.
    while (h-->0) {
      uint8_t *dsti=dstrow; dstrow+=dst->stride;
      const uint8_t *srci=srcrow; srcrow+=src->stride;
      uint8_t rgba[4];
      int i; for (i=0;i<w;i++) {
        rdpx(rgba,srci);
        wrpx(dsti,rgba);
        dsti+=dst->pixelsize;
        srci+=src->pixelsize;
      }
    }
  }
  return 0;
}

/* Receive sub-pixel image.
 */
 
int imgtl_image_read_1bit(struct imgtl_image *image,const void *src,int normalize) {
  if (!image||!src) return -1;
  if (!image->write_pixels) return -1;
  if (normalize) normalize=0xff; else normalize=0x01;
  uint8_t *dstrow=image->pixels;
  const uint8_t *srcrow=src;
  int srcstride=(image->w+7)>>3;
  int y; for (y=0;y<image->h;y++) {
    uint8_t *dsti=dstrow; dstrow+=image->stride;
    const uint8_t *srci=srcrow; srcrow+=srcstride;
    uint8_t mask=0x80;
    int x; for (x=0;x<image->w;x++,dsti+=image->pixelsize) {
      if (*srci&mask) *dsti=normalize; else *dsti=0x00;
      if (!(mask>>=1)) { mask=0x80; srci++; }
    }
  }
  return 0;
}

int imgtl_image_read_2bit(struct imgtl_image *image,const void *src,int normalize) {
  if (!image||!src) return -1;
  if (!image->write_pixels) return -1;
  uint8_t values[4]={0x00,0x55,0xaa,0xff};
  if (!normalize) { values[0]=0; values[1]=1; values[2]=2; values[3]=3; }
  uint8_t *dstrow=image->pixels;
  const uint8_t *srcrow=src;
  int srcstride=(image->w+3)>>2;
  int y; for (y=0;y<image->h;y++) {
    uint8_t *dsti=dstrow; dstrow+=image->stride;
    const uint8_t *srci=srcrow; srcrow+=srcstride;
    int shift=6;
    int x; for (x=0;x<image->w;x++,dsti+=image->pixelsize) {
      *dsti=values[(*srci>>shift)&3];
      if (shift) shift-=2; else { shift=6; srci++; }
    }
  }
  return 0;
}

int imgtl_image_read_4bit(struct imgtl_image *image,const void *src,int normalize) {
  if (!image||!src) return -1;
  if (!image->write_pixels) return -1;
  uint8_t *dstrow=image->pixels;
  const uint8_t *srcrow=src;
  int srcstride=(image->w+1)>>1;
  int y; for (y=0;y<image->h;y++) {
    uint8_t *dsti=dstrow; dstrow+=image->stride;
    const uint8_t *srci=srcrow; srcrow+=srcstride;
    int x; for (x=0;x<image->w;x++,dsti+=image->pixelsize) {
      int value;
      if (x&1) value=*srci>>4; else { value=*srci&15; srci++; }
      if (normalize) value|=value<<4;
      *dsti=value;
    }
  }
  return 0;
}
