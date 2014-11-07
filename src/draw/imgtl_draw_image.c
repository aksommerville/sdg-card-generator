#include "imgtl.h"
#include "imgtl_draw.h"
#include "image/imgtl_image.h"

/* Draw image, main entry point.
 */
 
int imgtl_draw_image(struct imgtl_image *dst,int dstx,int dsty,const struct imgtl_image *src,int srcx,int srcy,int w,int h) {

  // Validate and clip.
  if (!dst||!src) return -1;
  if (!dst->write_pixels) return -1;
  if ((w<1)||(h<1)) return 0;
  if (dstx<0) { if ((w+=dstx)<1) return 0; srcx-=dstx; dstx=0; }
  if (dsty<0) { if ((h+=dsty)<1) return 0; srcy-=dsty; dsty=0; }
  if (srcx<0) { if ((w+=srcx)<1) return 0; dstx-=srcx; srcx=0; }
  if (srcy<0) { if ((h+=srcy)<1) return 0; dsty-=srcy; srcy=0; }
  if (dstx>=dst->w) return 0;
  if (dsty>=dst->h) return 0;
  if (srcx>=src->w) return 0;
  if (srcy>=src->h) return 0;
  if (dstx>dst->w-w) w=dst->w-dstx;
  if (dsty>dst->h-h) h=dst->h-dsty;
  if (srcx>src->w-w) w=src->w-srcx;
  if (srcy>src->h-h) h=src->h-srcy;

  // Iterate over pixels.
  imgtl_rgba_from_pixel_fn rdpx=imgtl_fmt_get_reader(src->fmt);
  imgtl_pixel_from_rgba_fn wrpx=imgtl_fmt_get_writer(dst->fmt);
  imgtl_rgba_from_pixel_fn drdpx=imgtl_fmt_get_reader(dst->fmt);
  if (!rdpx||!wrpx||!drdpx) return -1;
  uint8_t *dstrow=(uint8_t*)dst->pixels+dsty*dst->stride+dstx*dst->pixelsize;
  const uint8_t *srcrow=(uint8_t*)src->pixels+srcy*src->stride+srcx*src->pixelsize;
  while (h-->0) {
    uint8_t *dsti=dstrow; dstrow+=dst->stride;
    const uint8_t *srci=srcrow; srcrow+=src->stride;
    uint8_t rgba[4],rgba2[4];
    int i; for (i=0;i<w;i++) {
      rdpx(rgba,srci);
      if (rgba[3]>0x00) {
        if (rgba[3]<0xff) {
          drdpx(rgba2,dsti);
          if (rgba2[3]>0x00) {
            int asum=rgba[3]+rgba2[3];
            if (asum>0xff) {
              rgba2[3]=0xff-rgba[3];
              asum=0xff;
            }
            rgba[0]=(rgba[0]*rgba[3]+rgba2[0]*rgba2[3])>>8;
            rgba[1]=(rgba[1]*rgba[3]+rgba2[1]*rgba2[3])>>8;
            rgba[2]=(rgba[2]*rgba[3]+rgba2[2]*rgba2[3])>>8;
            rgba[3]=asum;
            wrpx(dsti,rgba);
          } else {
            wrpx(dsti,rgba);
          }
        } else {
          wrpx(dsti,rgba);
        }
      }
      dsti+=dst->pixelsize;
      srci+=src->pixelsize;
    }
  }

  return 0;
}

/* Simple transform.
 */
 
struct imgtl_image *imgtl_xform(struct imgtl_image *src,int xform) {
  if (!src) return 0;
  struct imgtl_image *dst=0;
  switch (xform) {
    case IMGTL_XFORM_NONE: return imgtl_image_copy(-1,src,0,0,-1,-1);
    case IMGTL_XFORM_CLOCK:
    case IMGTL_XFORM_COUNTER:
    case IMGTL_XFORM_FLOPCLOCK:
    case IMGTL_XFORM_FLOPCOUNTER: {
        dst=imgtl_image_new_alloc(src->h,src->w,src->fmt);
      } break;
    case IMGTL_XFORM_180:
    case IMGTL_XFORM_FLOP:
    case IMGTL_XFORM_FLIP: {
        dst=imgtl_image_new_alloc(src->w,src->h,src->fmt);
      } break;
    default: return 0;
  }
  const uint8_t *srcrow=src->pixels;
  int y; for (y=0;y<src->h;y++) {
    const uint8_t *srci=srcrow; srcrow+=src->stride;
    int x; for (x=0;x<src->w;x++,srci+=src->pixelsize) {
      int dstx,dsty;
      switch (xform) {
        case IMGTL_XFORM_CLOCK: dstx=src->h-y-1; dsty=x; break;
        case IMGTL_XFORM_COUNTER: dstx=y; dsty=src->w-x-1; break;
        case IMGTL_XFORM_FLOPCLOCK: dstx=src->h-y-1; dsty=src->w-x-1; break;
        case IMGTL_XFORM_FLOPCOUNTER: dstx=y; dsty=x; break;
        case IMGTL_XFORM_180: dstx=src->w-x-1; dsty=src->h-y-1; break;
        case IMGTL_XFORM_FLOP: dstx=src->w-x-1; dsty=y; break;
        case IMGTL_XFORM_FLIP: dstx=x; dsty=src->h-y-1; break;
        default: imgtl_image_del(dst); return 0;
      }
      uint8_t *dsti=(uint8_t*)dst->pixels+dsty*dst->stride+dstx*dst->pixelsize;
      memcpy(dsti,srci,src->pixelsize);
    }
  }
  return dst;
}

/* Draw alpha.
 */
 
int imgtl_draw_a8(struct imgtl_image *image,int x,int y,const void *alpha,int srcstride,int w,int h,uint32_t rgba) {
  if ((w<1)||(h<1)) return 0;
  if (!image||!alpha) return -1;
  uint8_t r=rgba>>24,g=rgba>>16,b=rgba>>8,a=rgba;
  if (!a) return 0;
  int srcx=0,srcy=0;
  if (x<0) { if ((w+=x)<1) return 0; srcx-=x; x=0; }
  if (y<0) { if ((h+=y)<1) return 0; srcy-=y; y=0; }
  if (x>=image->w) return 0;
  if (y>=image->h) return 0;
  if (x>image->w-w) w=image->w-x;
  if (y>image->h-h) h=image->h-y;
  const uint8_t *srcrow=(uint8_t*)alpha+srcy*srcstride+srcx;
  uint8_t *dstrow=(uint8_t*)image->pixels+y*image->stride+x*image->pixelsize;
  imgtl_rgba_from_pixel_fn rdpx=imgtl_fmt_get_reader(image->fmt);
  imgtl_pixel_from_rgba_fn wrpx=imgtl_fmt_get_writer(image->fmt);
  if (!rdpx||!wrpx) return -1;
  while (h-->0) {
    const uint8_t *srci=srcrow; srcrow+=srcstride;
    uint8_t *dsti=dstrow; dstrow+=image->stride;
    int i; for (i=0;i<w;i++,dsti+=image->pixelsize,srci++) {
      if (!*srci) continue;
      uint8_t tmp[4];
      if ((*srci==0xff)&&(a==0xff)) {
        tmp[0]=r; tmp[1]=g; tmp[2]=b; tmp[3]=0xff;
        wrpx(dsti,tmp);
      } else {
        int srca=(*srci*a)>>8;
        if (!srca) continue;
        int dsta=0xff-srca;
        rdpx(tmp,dsti);
        tmp[0]=(tmp[0]*dsta+r*srca)>>8;
        tmp[1]=(tmp[1]*dsta+g*srca)>>8;
        tmp[2]=(tmp[2]*dsta+b*srca)>>8;
        tmp[3]=0xff;
        wrpx(dsti,tmp);
      }
    }
  }
  return 0;
}
