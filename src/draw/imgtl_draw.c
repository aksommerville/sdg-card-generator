#include "imgtl.h"
#include "imgtl_draw.h"
#include "image/imgtl_image.h"

/* Fill rectangle.
 */
 
int imgtl_draw_rect(struct imgtl_image *image,int x,int y,int w,int h,uint32_t rgba) {
  uint8_t pixel[8];
  if (imgtl_pixel_from_rgba(pixel,image,rgba)<0) return -1;
  if ((w<1)||(h<1)) return 0;
  if (x<0) { if ((w+=x)<1) return 0; x=0; }
  if (y<0) { if ((h+=y)<1) return 0; y=0; }
  if (x>=image->w) return 0;
  if (y>=image->h) return 0;
  if (x>image->w-w) w=image->w-x;
  if (y>image->h-h) h=image->h-y;
  uint8_t *row=(uint8_t*)image->pixels+y*image->stride+x*image->pixelsize;
  if (image->pixelsize==1) {
    while (h-->0) {
      memset(row,pixel[0],w);
      row+=image->stride;
    }
  } else {
    while (h-->0) {
      uint8_t *dst=row; row+=image->stride;
      int i; for (i=0;i<w;i++,dst+=image->pixelsize) memcpy(dst,pixel,image->pixelsize);
    }
  }
  return 0;
}

/* Draw line.
 */
 
int imgtl_draw_line(struct imgtl_image *image,int ax,int ay,int bx,int by,uint32_t rgba) {

  // Get out immediately if aligned to axis.
  if (ax==bx) {
    if (ay<by) return imgtl_draw_rect(image,ax,ay,1,by-ay+1,rgba);
    return imgtl_draw_rect(image,ax,by,1,ay-by+1,rgba);
  } else if (ay==by) {
    if (ax<bx) return imgtl_draw_rect(image,ax,ay,bx-ax+1,1,rgba);
    return imgtl_draw_rect(image,bx,ay,ax-bx+1,1,rgba);
  }

  // Prepare.
  uint8_t pixel[8];
  if (imgtl_pixel_from_rgba(pixel,image,rgba)<0) return -1;
  int weight,xweight,yweight,dx,dy,x,y;
  if (ax<bx) { xweight=bx-ax; dx=1; }
  else { xweight=ax-bx; dx=-1; }
  if (ay<by) { yweight=ay-by; dy=1; }
  else { yweight=by-ay; dy=-1; }
  int xthresh=xweight>>1; if (!xthresh) xthresh=1;
  int ythresh=yweight>>1;
  weight=xweight+yweight;
  x=ax; y=ay;

  // Walk it.
  while (1) {
    if ((x>=0)&&(y>=0)&&(x<image->w)&&(y<image->h)) {
      memcpy((uint8_t*)image->pixels+y*image->stride+x*image->pixelsize,pixel,image->pixelsize);
    }
    if ((x==bx)&&(y==by)) break;
    if ((weight>=xthresh)&&(x!=bx)) { weight+=yweight; x+=dx; }
    else if ((weight<=ythresh)&&(y!=by)) { weight+=xweight; y+=dy; }
    else {
      weight+=xweight+yweight;
      if (x!=bx) x+=dx;
      if (y!=by) y+=dy;
    }
  }

  return 0;
}
