#include "imgtl.h"
#include "imgtl_image.h"
#include "misc/imgtl_text.h"
#include "misc/imgtl_fs.h"
#include "misc/imgtl_buffer.h"
#include "serial/imgtl_png.h"
#include <errno.h>

int imgtl_image_save_hint=IMGTL_IMAGE_SAVE_HINT_DEFAULT;

/* Decode raw image data.
 * Path must contain format and dimensions, eg "my-image.64x32.rgba"
 * **** Unlike all other decoders, this one hands off data rather than copying it. ****
 */

static struct imgtl_image *imgtl_image_decode_raw(void *src,int srcc,const char *path) {
  int w,h,fmt,imgsize;
  if ((imgsize=imgtl_raw_path_eval(&w,&h,&fmt,path,-1))<1) {
    fprintf(stderr,"%s:ERROR: Unable to determine image format.\n",path);
    return 0;
  }
  if (imgsize>srcc) {
    fprintf(stderr,"%s:ERROR: Expected %d bytes based on format and dimensions, found %d.\n",path,imgsize,srcc);
    return 0;
  }
  struct imgtl_image *image=imgtl_image_new_handoff(src,w,h,fmt,0);
  if (!image) return 0;
  return image;
}

/* Decode PNG.
 */

static struct imgtl_image *imgtl_image_decode_png_expand(struct imgtl_png *png,int (*expand)(struct imgtl_image *image,const void *src,int normalize),int normalize) {
  int fmt;
  if (normalize) {
    fmt=IMGTL_FMT_Y8;
  } else {
    if (png->pltec>=3) fmt=IMGTL_FMT_RGB;
    else { normalize=1; fmt=IMGTL_FMT_Y8; }
  }
  struct imgtl_image *image=imgtl_image_new_alloc(png->w,png->h,fmt);
  if (!image) return 0;
  if (expand) {
    if (expand(image,png->pixels,normalize)<0) { imgtl_image_del(image); return 0; }
  }
  if (fmt==IMGTL_FMT_RGB) { // Expand color table.
    const uint8_t *PLTE=png->plte;
    uint8_t *dst=image->pixels;
    int pixelc=image->w*image->h;
    while (pixelc-->0) {
      int ix=*dst*3;
      if (ix>png->pltec-3) ix=0;
      *dst++=PLTE[ix++];
      *dst++=PLTE[ix++];
      *dst++=PLTE[ix];
    }
  }
  return image;
}

static struct imgtl_image *imgtl_image_decode_png(const void *src,int srcc,const char *path) {

  // Decode PNG file.
  struct imgtl_png png={0};
  if (imgtl_png_decode(&png,src,srcc)<0) {
    if (png.msgc) fprintf(stderr,"%s:ERROR: %s\n",path,png.msg);
    else fprintf(stderr,"%s:ERROR: Failed to decode PNG.\n",path);
    imgtl_png_cleanup(&png);
    return 0;
  }

  // Create image based on known formats.
  struct imgtl_image *image=0;
  switch (png.colortype) {
    case 0: switch (png.depth) {
        case 1: image=imgtl_image_decode_png_expand(&png,imgtl_image_read_1bit,1); break;
        case 2: image=imgtl_image_decode_png_expand(&png,imgtl_image_read_2bit,1); break;
        case 4: image=imgtl_image_decode_png_expand(&png,imgtl_image_read_4bit,1); break;
        case 8: image=imgtl_image_new_handoff(png.pixels,png.w,png.h,IMGTL_FMT_Y8,0); break;
      } break;
    case 2: switch (png.depth) {
        case 8: image=imgtl_image_new_handoff(png.pixels,png.w,png.h,IMGTL_FMT_RGB,0); break;
      } break;
    case 3: switch (png.depth) {
        case 1: image=imgtl_image_decode_png_expand(&png,imgtl_image_read_1bit,0); break;
        case 2: image=imgtl_image_decode_png_expand(&png,imgtl_image_read_2bit,0); break;
        case 4: image=imgtl_image_decode_png_expand(&png,imgtl_image_read_4bit,0); break;
        case 8: image=imgtl_image_decode_png_expand(&png,0,0); break;
      } break;
    case 6: switch (png.depth) {
        case 8: image=imgtl_image_new_handoff(png.pixels,png.w,png.h,IMGTL_FMT_RGBA,0); break;
      } break;
  }

  // If that didn't work, try our "unspecified" formats.
  if (!image) switch (png.pixelsize) {
    case  1: image=imgtl_image_decode_png_expand(&png,imgtl_image_read_1bit,1); break;
    case  2: image=imgtl_image_decode_png_expand(&png,imgtl_image_read_2bit,1); break;
    case  4: image=imgtl_image_decode_png_expand(&png,imgtl_image_read_4bit,1); break;
    case  8: image=imgtl_image_new_handoff(png.pixels,png.w,png.h,IMGTL_FMT_1,0); break;
    case 16: image=imgtl_image_new_handoff(png.pixels,png.w,png.h,IMGTL_FMT_2,0); break;
    case 24: image=imgtl_image_new_handoff(png.pixels,png.w,png.h,IMGTL_FMT_3,0); break;
    case 32: image=imgtl_image_new_handoff(png.pixels,png.w,png.h,IMGTL_FMT_4,0); break;
    case 40: image=imgtl_image_new_handoff(png.pixels,png.w,png.h,IMGTL_FMT_5,0); break;
    case 48: image=imgtl_image_new_handoff(png.pixels,png.w,png.h,IMGTL_FMT_6,0); break;
    case 56: image=imgtl_image_new_handoff(png.pixels,png.w,png.h,IMGTL_FMT_7,0); break;
    case 64: image=imgtl_image_new_handoff(png.pixels,png.w,png.h,IMGTL_FMT_8,0); break;
  }
  
  // Still haven't got it, we're done.
  if (!image) {
    fprintf(stderr,"%s:ERROR: Failed to convert PNG image to usable format.\n",path);
    imgtl_png_cleanup(&png);
    return 0;
  }

  // If we borrowed PNG's pixels, be sure to zero them!
  if (image->pixels==png.pixels) png.pixels=0;
  imgtl_png_cleanup(&png);
  return image;
}

/* Decode formats not yet implemented.
 */

static struct imgtl_image *imgtl_image_decode_gif(const void *src,int srcc,const char *path) {
  fprintf(stderr,"%s:ERROR: GIF not yet supported.\n",path);
  return 0;
}

static struct imgtl_image *imgtl_image_decode_jpeg(const void *src,int srcc,const char *path) {
  fprintf(stderr,"%s:ERROR: JPEG not yet supported.\n",path);
  return 0;
}

static struct imgtl_image *imgtl_image_decode_bmp(const void *src,int srcc,const char *path) {
  fprintf(stderr,"%s:ERROR: BMP not yet supported.\n",path);
  return 0;
}

/* Load file.
 */
 
struct imgtl_image *imgtl_image_load_file(const char *path) {

  // Acquire serial data.
  void *src=0;
  errno=ENOMEM;
  int srcc=imgtl_file_read(&src,path);
  if ((srcc<0)||!src) {
    fprintf(stderr,"%s:ERROR: %s\n",path,strerror(errno));
    return 0;
  }
  struct imgtl_image *image=0;

  // Look for unambiguous signatures in content.
  if ((srcc>=8)&&!memcmp(src,"\x89PNG\r\n\x1a\n",8)) {
    image=imgtl_image_decode_png(src,srcc,path);
  } else if ((srcc>=6)&&(!memcmp(src,"GIF87a",6)||!memcmp(src,"GIF89a",6))) {
    image=imgtl_image_decode_gif(src,srcc,path);

  // Consider path suffix. (though of course, if we didn't see a signature, nothing is likely to work here).
  } else {
    const char *ext=0; int extc=0,pathp=0;
    for (;path[pathp];pathp++) {
      if (path[pathp]=='/') { ext=0; extc=0; }
      else if (path[pathp]=='.') { ext=path+pathp+1; extc=0; }
      else if (ext) extc++;
    }
    if ((extc==3)&&!imgtl_memcasecmp(ext,"png",3)) {
      image=imgtl_image_decode_png(src,srcc,path);
    } else if ((extc==3)&&!imgtl_memcasecmp(ext,"gif",3)) {
      image=imgtl_image_decode_gif(src,srcc,path);
    } else if ((extc==4)&&!imgtl_memcasecmp(ext,"jpeg",4)) {
      image=imgtl_image_decode_jpeg(src,srcc,path);
    } else if ((extc==3)&&!imgtl_memcasecmp(ext,"jpg",3)) {
      image=imgtl_image_decode_jpeg(src,srcc,path);
    } else if ((extc==3)&&!imgtl_memcasecmp(ext,"bmp",3)) {
      image=imgtl_image_decode_bmp(src,srcc,path);
    } else {
      if (image=imgtl_image_decode_raw(src,srcc,path)) src=0;
    }
  }

  if (src) free(src);
  return image;
}

/* Save PNG.
 */

static int imgtl_image_save_png(const char *path,const struct imgtl_image *image) {
  struct imgtl_png png={0};
  
  switch (image->fmt) {
    case IMGTL_FMT_1: png.colortype=0; png.depth=8; break;
    case IMGTL_FMT_2: png.colortype=4; png.depth=8; break;
    case IMGTL_FMT_3: png.colortype=2; png.depth=8; break;
    case IMGTL_FMT_4: png.colortype=6; png.depth=8; break;
    case IMGTL_FMT_6: png.colortype=2; png.depth=16; break;
    case IMGTL_FMT_8: png.colortype=6; png.depth=16; break;
    case IMGTL_FMT_RGB: png.colortype=2; png.depth=8; break;
    case IMGTL_FMT_RGBA: png.colortype=6; png.depth=8; break;
    case IMGTL_FMT_Y8: png.colortype=0; png.depth=8; break;
    case IMGTL_FMT_A8: png.colortype=0; png.depth=8; break;
    default: {
        struct imgtl_image *rgba=imgtl_image_copy(IMGTL_FMT_RGBA,image,0,0,image->w,image->h);
        if (!rgba) return -1;
        int err=imgtl_image_save_png(path,rgba);
        imgtl_image_del(rgba);
        return err;
      }
  }

  switch (imgtl_image_save_hint) {
    case IMGTL_IMAGE_SAVE_HINT_DEFAULT: break;
    case IMGTL_IMAGE_SAVE_HINT_FASTER: {
        png.filter_strategy=IMGTL_PNG_FILTER_ALWAYS_NONE;
      } break;
    case IMGTL_IMAGE_SAVE_HINT_MUCH_FASTER: {
        png.filter_strategy=IMGTL_PNG_FILTER_ALWAYS_NONE;
        png.flags|=IMGTL_PNG_UNCOMPRESSED;
      } break;
  }
  
  png.w=image->w;
  png.h=image->h;
  int stride=imgtl_fmt_stride(image->fmt,image->w);
  if (stride!=image->stride) {
    struct imgtl_image *flat=imgtl_image_copy(image->fmt,image,0,0,image->w,image->h);
    if (!flat) return -1;
    if (stride!=flat->stride) { imgtl_image_del(flat); return -1; }
    int err=imgtl_image_save_png(path,flat);
    imgtl_image_del(flat);
    return err;
  }
  png.pixels=image->pixels;
  struct imgtl_buffer buffer={0};
  int err=imgtl_png_encode(&buffer,&png);
  png.pixels=0;
  if (err<0) {
    fprintf(stderr,"%s:ERROR: Failed to encode PNG: %s.\n",path,png.msg);
    imgtl_buffer_cleanup(&buffer);
    imgtl_png_cleanup(&png);
    return -1;
  }
  imgtl_png_cleanup(&png);
  err=imgtl_file_write(path,buffer.v,buffer.c);
  if (err>=0) err=buffer.c;
  imgtl_buffer_cleanup(&buffer);
  return err;
}

/* Save other formats.
 */

static int imgtl_image_save_gif(const char *path,const struct imgtl_image *image) {
  fprintf(stderr,"%s:ERROR: GIF not yet implemented.\n",path);
  return -1;
}

static int imgtl_image_save_jpeg(const char *path,const struct imgtl_image *image) {
  fprintf(stderr,"%s:ERROR: JPEG not yet implemented.\n",path);
  return -1;
}

static int imgtl_image_save_bmp(const char *path,const struct imgtl_image *image) {
  fprintf(stderr,"%s:ERROR: BMP not yet implemented.\n",path);
  return -1;
}

/* Save file.
 */
 
int imgtl_image_save_file(const char *path,const struct imgtl_image *image) {
  if (!path||!image) return -1;

  // Raw images are super easy. Caller must provide exact dimensions and format in path.
  int w,h,fmt,imgsize;
  if ((imgsize=imgtl_raw_path_eval(&w,&h,&fmt,path,-1))>0) {
    if ((w!=image->w)||(h!=image->h)||(fmt!=image->fmt)) return -1;
    if (imgtl_file_write(path,image->pixels,imgsize)<0) return -1;
    return imgsize;
  }

  // Take a hint for the format from the provided path.
  const char *ext=0; int extc=0,pathp=0;
  for (;path[pathp];pathp++) {
    if (path[pathp]=='/') { ext=0; extc=0; }
    else if (path[pathp]=='.') { ext=path+pathp+1; extc=0; }
    else if (ext) extc++;
  }

  if ((extc==3)&&!imgtl_memcasecmp(ext,"png",3)) return imgtl_image_save_png(path,image);
  if ((extc==3)&&!imgtl_memcasecmp(ext,"gif",3)) return imgtl_image_save_gif(path,image);
  if ((extc==4)&&!imgtl_memcasecmp(ext,"jpeg",4)) return imgtl_image_save_jpeg(path,image);
  if ((extc==3)&&!imgtl_memcasecmp(ext,"jpg",3)) return imgtl_image_save_jpeg(path,image);
  if ((extc==3)&&!imgtl_memcasecmp(ext,"bmp",3)) return imgtl_image_save_bmp(path,image);

  // If we still can't tell, issue a warning and use PNG.
  fprintf(stderr,"%s:WARNING: Saving to file name with ambiguous extension. Using PNG format.\n",path);
  return imgtl_image_save_png(path,image);
}
