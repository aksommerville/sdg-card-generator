#include "imgtl.h"
#include "imgtl_image.h"

/* Delete and retain.
 */
 
void imgtl_image_del(struct imgtl_image *image) {
  if (!image) return;
  if (image->refc>1) { image->refc--; return; }
  if (image->pixels&&image->free_pixels) free(image->pixels);
  if (image->keepalive) imgtl_image_del(image->keepalive);
  free(image);
}

int imgtl_image_ref(struct imgtl_image *image) {
  if (!image) return -1;
  if (image->refc<1) return -1;
  if (image->refc==INT_MAX) return -1;
  image->refc++;
  return 0;
}

/* Basic ctors.
 */
 
struct imgtl_image *imgtl_image_new_alloc(int w,int h,int fmt) {
  if (h<1) return 0;
  int stride=imgtl_fmt_stride(fmt,w);
  if (stride<1) return 0;
  if (stride>INT_MAX/h) return 0;
  void *pixels=calloc(stride,h);
  if (!pixels) return 0;
  struct imgtl_image *image=imgtl_image_new_handoff(pixels,w,h,fmt,stride);
  if (!image) { free(pixels); return 0; }
  return image;
}

struct imgtl_image *imgtl_image_new_handoff(void *pixels,int w,int h,int fmt,int stride) {
  if (!pixels||(h<1)) return 0;
  int minstride=imgtl_fmt_stride(fmt,w);
  if (minstride<1) return 0;
  if (stride<1) stride=minstride;
  else if (stride<minstride) return 0;
  if (stride>INT_MAX/h) return 0;
  struct imgtl_image *image=calloc(1,sizeof(struct imgtl_image));
  if (!image) return 0;
  image->refc=1;
  image->w=w;
  image->h=h;
  image->fmt=fmt;
  image->stride=stride;
  image->pixelsize=imgtl_fmt_size(fmt);
  image->pixels=pixels;
  image->free_pixels=1;
  image->write_pixels=1;
  return image;
}

struct imgtl_image *imgtl_image_new_borrow(const void *pixels,int w,int h,int fmt,int stride,int writeable) {
  struct imgtl_image *image=imgtl_image_new_handoff((void*)pixels,w,h,fmt,stride);
  if (!image) return 0;
  image->free_pixels=0;
  if (!writeable) image->write_pixels=0;
  return image;
}

/* Detect retention loops.
 */

int imgtl_image_refers(const struct imgtl_image *src,const struct imgtl_image *query) {
  if (!query) return 0;
  while (src) {
    if (src==query) return 1;
    src=src->keepalive;
  }
  return 0;
}

/* Create child image.
 */
 
struct imgtl_image *imgtl_image_sub(struct imgtl_image *src,int x,int y,int w,int h) {
  if (!src) return 0;
  if ((x<0)||(y<0)||(w<1)||(h<1)||(x>src->w-w)||(y>src->h-h)) return 0; // Bad boundary.
  char *pixels=src->pixels+y*src->stride+x*src->pixelsize;
  struct imgtl_image *child=imgtl_image_new_borrow(pixels,w,h,src->fmt,src->stride,src->write_pixels);
  if (!child) return 0;
  if (imgtl_image_ref(src)<0) { imgtl_image_del(child); return 0; }
  child->keepalive=src;
  return child;
}

/* Copy into new image.
 */
 
struct imgtl_image *imgtl_image_copy(int dstfmt,const struct imgtl_image *src,int x,int y,int w,int h) {
  if (!src) return 0;
  if (dstfmt<0) dstfmt=src->fmt;
  if (w<1) { if ((w=src->w-x)<1) return 0; }
  if (h<1) { if ((h=src->h-y)<1) return 0; }
  if (x+w<1) return 0;
  if (y+h<1) return 0;
  if (x>=src->w) return 0;
  if (y>=src->h) return 0;
  struct imgtl_image *dst=imgtl_image_new_alloc(w,h,dstfmt);
  if (!dst) return 0;
  if (imgtl_copy(dst,0,0,src,x,y,w,h)<0) { imgtl_image_del(dst); return 0; }
  return dst;
}

/* Convert color.
 */
 
int imgtl_pixel_from_rgba(uint8_t *pixel,const struct imgtl_image *image,uint32_t rgba) {
  if (!pixel||!image) return -1;
  imgtl_pixel_from_rgba_fn fn=imgtl_fmt_get_writer(image->fmt);
  if (!fn) return -1;
  uint8_t src[4]={rgba>>24,rgba>>16,rgba>>8,rgba};
  fn(pixel,src);
  return 0;
}

int imgtl_rgba_from_pixel(uint32_t *rgba,const struct imgtl_image *image,const uint8_t *pixel) {
  if (!rgba||!image||!pixel) return -1;
  imgtl_rgba_from_pixel_fn fn=imgtl_fmt_get_reader(image->fmt);
  if (!fn) return -1;
  uint8_t dst[4];
  fn(dst,pixel);
  *rgba=(dst[0]<<24)|(dst[1]<<16)|(dst[2]<<8)|dst[3];
  return 0;
}
