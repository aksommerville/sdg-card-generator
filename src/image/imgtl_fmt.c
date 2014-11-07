#include "imgtl.h"
#include "misc/imgtl_text.h"
#include "imgtl_image.h"

/* Size.
 */
 
int imgtl_fmt_size(int fmt) {
  switch (fmt) {
    case IMGTL_FMT_1: return 1;
    case IMGTL_FMT_2: return 2;
    case IMGTL_FMT_3: return 3;
    case IMGTL_FMT_4: return 4;
    case IMGTL_FMT_5: return 5;
    case IMGTL_FMT_6: return 6;
    case IMGTL_FMT_7: return 7;
    case IMGTL_FMT_8: return 8;
    case IMGTL_FMT_RGB: return 3;
    case IMGTL_FMT_RGBA: return 4;
    case IMGTL_FMT_Y8: return 1;
    case IMGTL_FMT_A8: return 1;
  }
  return -1;
}

int imgtl_fmt_stride(int fmt,int w) {
  if (w<1) return -1;
  int size=imgtl_fmt_size(fmt);
  if (size<0) return -1;
  if (w>INT_MAX/size) return -1;
  return w*size;
}

/* Colorspace.
 */

int imgtl_fmt_colorspace(int fmt) {
  switch (fmt) {
    case IMGTL_FMT_1: return IMGTL_CS_UNSPEC;
    case IMGTL_FMT_2: return IMGTL_CS_UNSPEC;
    case IMGTL_FMT_3: return IMGTL_CS_UNSPEC;
    case IMGTL_FMT_4: return IMGTL_CS_UNSPEC;
    case IMGTL_FMT_5: return IMGTL_CS_UNSPEC;
    case IMGTL_FMT_6: return IMGTL_CS_UNSPEC;
    case IMGTL_FMT_7: return IMGTL_CS_UNSPEC;
    case IMGTL_FMT_8: return IMGTL_CS_UNSPEC;
    case IMGTL_FMT_RGB: return IMGTL_CS_RGB;
    case IMGTL_FMT_RGBA: return IMGTL_CS_RGBA;
    case IMGTL_FMT_Y8: return IMGTL_CS_Y;
    case IMGTL_FMT_A8: return IMGTL_CS_A;
  }
  return -1;
}

/* Name.
 */

const char *imgtl_fmt_repr(int fmt) {
  switch (fmt) {
    case IMGTL_FMT_1: return "x1";
    case IMGTL_FMT_2: return "x2";
    case IMGTL_FMT_3: return "x3";
    case IMGTL_FMT_4: return "x4";
    case IMGTL_FMT_5: return "x5";
    case IMGTL_FMT_6: return "x6";
    case IMGTL_FMT_7: return "x7";
    case IMGTL_FMT_8: return "x8";
    case IMGTL_FMT_RGB: return "rgb";
    case IMGTL_FMT_RGBA: return "rgba";
    case IMGTL_FMT_Y8: return "y8";
    case IMGTL_FMT_A8: return "a8";
  }
  return 0;
}

int imgtl_fmt_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  switch (srcc) {
    case 2: {
        if ((src[0]=='x')||(src[0]=='X')) switch (src[1]) {
          case '1': return IMGTL_FMT_1;
          case '2': return IMGTL_FMT_2;
          case '3': return IMGTL_FMT_3;
          case '4': return IMGTL_FMT_4;
          case '5': return IMGTL_FMT_5;
          case '6': return IMGTL_FMT_6;
          case '7': return IMGTL_FMT_7;
          case '8': return IMGTL_FMT_8;
        }
        if (!imgtl_memcasecmp(src,"y8",2)) return IMGTL_FMT_Y8;
        if (!imgtl_memcasecmp(src,"a8",2)) return IMGTL_FMT_A8;
      } break;
    case 3: {
        if (!imgtl_memcasecmp(src,"rgb",3)) return IMGTL_FMT_RGB;
      } break;
    case 4: {
        if (!imgtl_memcasecmp(src,"rgba",4)) return IMGTL_FMT_RGBA;
      } break;
  }
  return -1;
}

/* Pixel accessors.
 */

static void imgtl_pixr_y(uint8_t *rgba,const uint8_t *pixel) { rgba[0]=rgba[1]=rgba[2]=pixel[0]; rgba[3]=0xff; }
static void imgtl_pixr_ya(uint8_t *rgba,const uint8_t *pixel) { rgba[0]=rgba[1]=rgba[2]=pixel[0]; rgba[3]=pixel[1]; }
static void imgtl_pixr_rgb(uint8_t *rgba,const uint8_t *pixel) { rgba[0]=pixel[0]; rgba[1]=pixel[1]; rgba[2]=pixel[2]; rgba[3]=0xff; }
static void imgtl_pixr_rgba(uint8_t *rgba,const uint8_t *pixel) { rgba[0]=pixel[0]; rgba[1]=pixel[1]; rgba[2]=pixel[2]; rgba[3]=pixel[3]; }
static void imgtl_pixr_rxgxbx(uint8_t *rgba,const uint8_t *pixel) { rgba[0]=pixel[0]; rgba[1]=pixel[2]; rgba[2]=pixel[4]; rgba[3]=0xff; }
static void imgtl_pixr_rxgxbxa(uint8_t *rgba,const uint8_t *pixel) { rgba[0]=pixel[0]; rgba[1]=pixel[2]; rgba[2]=pixel[4]; rgba[3]=pixel[6]; }
static void imgtl_pixr_a(uint8_t *rgba,const uint8_t *pixel) { rgba[0]=rgba[1]=rgba[2]=0x00; rgba[3]=pixel[0]; }

static void imgtl_pixw_y(uint8_t *pixel,const uint8_t *rgba) { pixel[0]=(rgba[0]+rgba[1]+rgba[2])/3; }
static void imgtl_pixw_ya(uint8_t *pixel,const uint8_t *rgba) { pixel[0]=(rgba[0]+rgba[1]+rgba[2])/3; pixel[1]=rgba[3]; }
static void imgtl_pixw_rgb(uint8_t *pixel,const uint8_t *rgba) { pixel[0]=rgba[0]; pixel[1]=rgba[1]; pixel[2]=rgba[2]; }
static void imgtl_pixw_rgba(uint8_t *pixel,const uint8_t *rgba) { pixel[0]=rgba[0]; pixel[1]=rgba[1]; pixel[2]=rgba[2]; pixel[3]=rgba[3]; }
static void imgtl_pixw_rxgxbx(uint8_t *pixel,const uint8_t *rgba) { pixel[0]=pixel[1]=rgba[0]; pixel[2]=pixel[3]=rgba[1]; pixel[4]=pixel[5]=rgba[2]; }
static void imgtl_pixw_rxgxbxa(uint8_t *pixel,const uint8_t *rgba) { pixel[0]=pixel[1]=rgba[0]; pixel[2]=pixel[3]=rgba[1]; pixel[4]=pixel[5]=rgba[2]; pixel[6]=rgba[3]; }
static void imgtl_pixw_a(uint8_t *pixel,const uint8_t *rgba) { pixel[0]=rgba[3]; }
 
imgtl_rgba_from_pixel_fn imgtl_fmt_get_reader(int fmt) {
  switch (fmt) {
    case IMGTL_FMT_1: return imgtl_pixr_y;
    case IMGTL_FMT_2: return imgtl_pixr_ya;
    case IMGTL_FMT_3: return imgtl_pixr_rgb;
    case IMGTL_FMT_4: return imgtl_pixr_rgba;
    case IMGTL_FMT_5: return imgtl_pixr_rgba;
    case IMGTL_FMT_6: return imgtl_pixr_rxgxbx;
    case IMGTL_FMT_7: return imgtl_pixr_rxgxbxa;
    case IMGTL_FMT_8: return imgtl_pixr_rxgxbxa;
    case IMGTL_FMT_RGB: return imgtl_pixr_rgb;
    case IMGTL_FMT_RGBA: return imgtl_pixr_rgba;
    case IMGTL_FMT_Y8: return imgtl_pixr_y;
    case IMGTL_FMT_A8: return imgtl_pixr_a;
    default: return 0;
  }
}

imgtl_pixel_from_rgba_fn imgtl_fmt_get_writer(int fmt) {
  switch (fmt) {
    case IMGTL_FMT_1: return imgtl_pixw_y;
    case IMGTL_FMT_2: return imgtl_pixw_ya;
    case IMGTL_FMT_3: return imgtl_pixw_rgb;
    case IMGTL_FMT_4: return imgtl_pixw_rgba;
    case IMGTL_FMT_5: return imgtl_pixw_rgba;
    case IMGTL_FMT_6: return imgtl_pixw_rxgxbx;
    case IMGTL_FMT_7: return imgtl_pixw_rxgxbxa;
    case IMGTL_FMT_8: return imgtl_pixw_rxgxbxa;
    case IMGTL_FMT_RGB: return imgtl_pixw_rgb;
    case IMGTL_FMT_RGBA: return imgtl_pixw_rgba;
    case IMGTL_FMT_Y8: return imgtl_pixw_y;
    case IMGTL_FMT_A8: return imgtl_pixw_a;
    default: return 0;
  }
}

int imgtl_pixel_reader_is_verbatim(imgtl_rgba_from_pixel_fn fn) {
  if (fn==imgtl_pixr_rgba) return 1;
  return 0;
}

int imgtl_pixel_writer_is_verbatim(imgtl_pixel_from_rgba_fn fn) {
  if (fn==imgtl_pixw_rgba) return 1;
  return 0;
}

/* Evaluate path for raw image.
 */
 
int imgtl_raw_path_eval(int *w,int *h,int *fmt,const char *path,int pathc) {
  if (!w||!h||!fmt||!path) return -1;
  if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  while (pathc&&(path[pathc-1]=='/')) pathc--;
  const char *fmtsrc=path+pathc; int fmtsrcc=0;
  while (pathc&&(path[pathc-1]!='.')&&(path[pathc-1]!='/')) { pathc--; fmtsrc--; fmtsrcc++; }
  if ((*fmt=imgtl_fmt_eval(fmtsrc,fmtsrcc))<0) return -1;
  while (pathc&&(path[pathc-1]=='.')) pathc--;
  const char *dimsrc=path+pathc; int dimsrcc=0;
  while (pathc&&(path[pathc-1]!='.')&&(path[pathc-1]!='/')) { pathc--; dimsrc--; dimsrcc++; }
  int xp=-1,i; for (i=0;i<dimsrcc;i++) if (dimsrc[i]=='x') { xp=i; break; }
  if ((xp<0)||((*w=imgtl_decuint_eval(dimsrc,xp))<1)||((*h=imgtl_decuint_eval(dimsrc+xp+1,dimsrcc-xp-1))<1)) return -1;
  int stride=imgtl_fmt_stride(*fmt,*w);
  if (stride<1) return -1;
  if (stride>INT_MAX/(*h)) return -1;
  return stride*(*h);
}
