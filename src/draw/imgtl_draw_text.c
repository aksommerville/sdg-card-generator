#include "imgtl.h"
#include "imgtl_draw.h"
#include "image/imgtl_image.h"
#include "misc/imgtl_text.h"
#include <dirent.h>
#include "ft2build.h"
#include FT_FREETYPE_H

/* Globals.
 */

struct imgtl_glyph {
  int ch;
  int dx;
  int dy;
  int advance;
  struct imgtl_image *image;
};

static struct {
  FT_Library library;
  FT_Face face;
  int size;
  char *path;
  struct imgtl_glyph *glyphv;
  int glyphc,glypha;
  int high_ascender; // Largest of glyphv->dy
} imgtl_text={0};

/* Glyph list helpers.
 */

static void imgtl_glyph_cleanup(struct imgtl_glyph *glyph) {
  if (!glyph) return;
  imgtl_image_del(glyph->image);
}

static int imgtl_glyph_search(int ch) {
  int lo=0,hi=imgtl_text.glyphc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (ch<imgtl_text.glyphv[ck].ch) hi=ck;
    else if (ch>imgtl_text.glyphv[ck].ch) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int imgtl_glyph_insert(int p,int ch,struct imgtl_image *image) {
  if ((p<0)||(p>imgtl_text.glyphc)) return -1;
  if (p&&(ch<=imgtl_text.glyphv[p-1].ch)) return -1;
  if ((p<imgtl_text.glyphc)&&(ch>=imgtl_text.glyphv[p].ch)) return -1;
  if (imgtl_text.glyphc>=imgtl_text.glypha) {
    int na=imgtl_text.glypha+32;
    if (na>INT_MAX/sizeof(struct imgtl_glyph)) return -1;
    void *nv=realloc(imgtl_text.glyphv,sizeof(struct imgtl_glyph)*na);
    if (!nv) return -1;
    imgtl_text.glyphv=nv;
    imgtl_text.glypha=na;
  }
  if (imgtl_image_ref(image)<0) return -1;
  memmove(imgtl_text.glyphv+p+1,imgtl_text.glyphv+p,sizeof(struct imgtl_glyph)*(imgtl_text.glyphc-p));
  imgtl_text.glyphc++;
  struct imgtl_glyph *glyph=imgtl_text.glyphv+p;
  memset(glyph,0,sizeof(struct imgtl_glyph));
  glyph->ch=ch;
  glyph->dy=-image->h;
  glyph->advance=image->w;
  glyph->image=image;
  return 0;
}

static int imgtl_glyph_remove(int p) {
  if ((p<0)||(p>=imgtl_text.glyphc)) return -1;
  imgtl_glyph_cleanup(imgtl_text.glyphv+p);
  imgtl_text.glyphc--;
  memmove(imgtl_text.glyphv+p,imgtl_text.glyphv+p+1,sizeof(struct imgtl_glyph)*(imgtl_text.glyphc-p));
  return 0;
}

/* Init, quit.
 */

int imgtl_text_init() {
  if (!imgtl_text.library) {
    if (FT_Init_FreeType(&imgtl_text.library)) return -1;
  }
  return 0;
}

void imgtl_text_quit() {
  if (imgtl_text.path) free(imgtl_text.path);
  if (imgtl_text.face) FT_Done_Face(imgtl_text.face);
  if (imgtl_text.library) FT_Done_FreeType(imgtl_text.library);
  if (imgtl_text.glyphv) {
    while (imgtl_text.glyphc-->0) imgtl_glyph_cleanup(imgtl_text.glyphv+imgtl_text.glyphc);
    free(imgtl_text.glyphv);
  }
  memset(&imgtl_text,0,sizeof(imgtl_text));
}

/* Search a few well-known locations for fonts by short name.
 * Short name does not contain any dot or slash.
 */

static int imgtl_text_search_font_1(char *dst,int dsta,const char *pfx,const char *name,int namec) {
  DIR *dir=opendir(pfx);
  if (!dir) return -1;
  struct dirent *de;
  while (de=readdir(dir)) {
    if (imgtl_memcasecmp(de->d_name,name,namec)) continue;
    if (de->d_name[namec]!='.') continue;
    const char *sfx=de->d_name+namec+1;
    int sfxc=0; while (sfx[sfxc]) sfxc++;
    // OK font formats: ttf pfa ttc otf
    if ((sfxc==3)&&(
      !imgtl_memcasecmp(sfx,"ttf",3)||
      !imgtl_memcasecmp(sfx,"pfa",3)||
      !imgtl_memcasecmp(sfx,"ttc",3)||
      !imgtl_memcasecmp(sfx,"otf",3)
    )) {
      int basec=namec+1+sfxc;
      int pfxc=0; while (pfx[pfxc]) pfxc++;
      int dstc=pfxc+basec;
      if (dstc<=dsta) {
        memcpy(dst,pfx,pfxc);
        memcpy(dst+pfxc,de->d_name,basec);
        if (dstc<dsta) dst[dstc]=0;
      }
      return dstc;
    }
  }
  closedir(dir);
  return -1;
}

static int imgtl_text_search_font(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<1)||!src||(srcc<1)) return -1;

  const char *pfxv[]={ // List must end with a NULL. Each entry must begin and end with slash.
    "/Library/Fonts/",
    "/opt/X11/share/fonts/Type1/",
    "/opt/X11/share/fonts/TTF/",
  0};
  int i; for (i=0;pfxv[i];i++) {
    int dstc=imgtl_text_search_font_1(dst,dsta,pfxv[i],src,srcc);
    if (dstc>=0) return dstc;
  }
  
  return -1;
}

/* Setup.
 */
 
int imgtl_text_setup(const char *path,int size) {
  if (!path||(size<1)) return -1;

  char subpath[1024];
  int isshortname=1,i; for (i=0;path[i];i++) if ((path[i]=='.')||(path[i]=='/')) { isshortname=0; break; }
  if (isshortname) {
    int subpathc=imgtl_text_search_font(subpath,sizeof(subpath),path,i);
    if ((subpathc>0)&&(subpathc<sizeof(subpath))) path=subpath;
  }
  
  if (imgtl_text.path&&!strcmp(path,imgtl_text.path)&&(size==imgtl_text.size)) return 0;
  if (imgtl_text_init()<0) return -1;
  if (imgtl_text.face) { FT_Done_Face(imgtl_text.face); imgtl_text.face=0; }
  if (FT_New_Face(imgtl_text.library,path,0,&imgtl_text.face)) return -1;
  if (imgtl_text.path) free(imgtl_text.path);
  if (!(imgtl_text.path=strdup(path))) return -1;
  if (FT_Set_Pixel_Sizes(imgtl_text.face,0,size)) return -1;
  imgtl_text.size=size;
  while (imgtl_text.glyphc>0) imgtl_glyph_cleanup(imgtl_text.glyphv+(--(imgtl_text.glyphc)));
  imgtl_text.high_ascender=size>>1; // Low initial guess.
  imgtl_measure_text(0,0,0,"AW^yqp_",-1); // Try to preload some tall glyphs.
  return 0;
}

/* Draw text.
 */
 
int imgtl_draw_text(struct imgtl_image *image,int x,int y,int align,const char *src,int srcc,uint32_t rgba) {
  if (!image) return -1;
  if (!imgtl_text.face) {
    fprintf(stderr,"imgtl:Error: Requested to draw text but no font set.\n");
    return -1;
  }
  if (!(rgba&0xff)) return 0;
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) return 0;

  if (align>0) {
    int w=0; imgtl_measure_text(&w,0,0,src,srcc);
    x-=w;
  } else if (align==0) {
    int w=0; imgtl_measure_text(&w,0,0,src,srcc);
    x-=w>>1;
  }

  int x0=x;
  int srcp=0,ch,seqlen; while (srcp<srcc) {
    if ((seqlen=imgtl_utf8_decode(&ch,src+srcp,srcc-srcp))<1) { seqlen=1; ch='?'; }
    srcp+=seqlen;
    int dx,dy,advance;
    struct imgtl_image *glyph=imgtl_text_get_glyph(&dx,&dy,&advance,ch);
    if (!glyph) continue;
    if (imgtl_draw_a8(image,x+dx,y-dy,glyph->pixels,glyph->stride,glyph->w,glyph->h,rgba)<0) return -1;
    x+=advance;
  }
  return x-x0;
}

/* Measure text.
 */
 
int imgtl_measure_text(int *w,int *ascent,int *descent,const char *src,int srcc) {
  int _w; if (!w) w=&_w; *w=0;
  int _ascent; if (!ascent) ascent=&_ascent; *ascent=0;
  int _descent; if (!descent) descent=&_descent; *descent=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }

  int srcp=0,ch,seqlen,altw=0;
  while (srcp<srcc) {
    if ((seqlen=imgtl_utf8_decode(&ch,src+srcp,srcc-srcp))<1) { ch='?'; seqlen=1; }
    srcp+=seqlen;
    int dx,dy,advance;
    struct imgtl_image *glyph=imgtl_text_get_glyph(&dx,&dy,&advance,ch);
    if (!glyph) continue;
    if (dy>*ascent) *ascent=dy;
    int descent1=glyph->h-dy;
    if (descent1>*descent) *descent=descent1;
    altw=*w+dx+glyph->w;
    *w+=advance;
  }

  if (altw>*w) *w=altw;
  return *ascent+*descent;
}

/* Create image for glyph.
 */

static struct imgtl_image *imgtl_image_for_glyph(const uint8_t *src,int srcstride,int w,int h) {

  // FreeType delivers an empty image for whitespace glyphs.
  // That's sensible, but we don't like empty images. In this case, make a dummy 1x1 transparent image.
  if (!src&&!srcstride&&!w&&!w) return imgtl_image_new_alloc(1,1,IMGTL_FMT_A8);

  struct imgtl_image *image=imgtl_image_new_alloc(w,h,IMGTL_FMT_A8);
  if (!image) return 0;
  uint8_t *dst=image->pixels;
  while (h-->0) {
    memcpy(dst,src,w);
    dst+=image->stride;
    src+=srcstride;
  }
  return image;
}

/* Get glyph.
 */
 
struct imgtl_image *imgtl_text_get_glyph(int *dx,int *dy,int *advance,int ch) {
  int p=imgtl_glyph_search(ch);
  if (p<0) {
    p=-p-1;
    if (!imgtl_text.face) {
      fprintf(stderr,"imgtl:Error: Requested font metrics but no font loaded.\n");
      return 0;
    }
    int gindex=FT_Get_Char_Index(imgtl_text.face,ch);
    if (FT_Load_Glyph(imgtl_text.face,gindex,FT_LOAD_DEFAULT)) return 0;
    if (FT_Render_Glyph(imgtl_text.face->glyph,FT_RENDER_MODE_NORMAL)) return 0;
    if (imgtl_text.face->glyph->format!=FT_GLYPH_FORMAT_BITMAP) return 0;
    FT_Bitmap *bitmap=&imgtl_text.face->glyph->bitmap;
    if (bitmap->pixel_mode!=FT_PIXEL_MODE_GRAY) return 0;
    struct imgtl_image *image=imgtl_image_for_glyph(bitmap->buffer,bitmap->pitch,bitmap->width,bitmap->rows);
    if (!image) return 0;
    int err=imgtl_glyph_insert(p,ch,image);
    imgtl_image_del(image);
    if (err<0) return 0;
    imgtl_text.glyphv[p].dx=imgtl_text.face->glyph->bitmap_left;
    imgtl_text.glyphv[p].dy=imgtl_text.face->glyph->bitmap_top;
    imgtl_text.glyphv[p].advance=imgtl_text.face->glyph->linearHoriAdvance>>16;
    if (imgtl_text.glyphv[p].dy>imgtl_text.high_ascender) imgtl_text.high_ascender=imgtl_text.glyphv[p].dy;
  }
  struct imgtl_glyph *glyph=imgtl_text.glyphv+p;
  if (dx) *dx=glyph->dx;
  if (dy) *dy=glyph->dy;
  if (advance) *advance=glyph->advance;
  return glyph->image;
}

/* Draw multiline text.
 */
 
int imgtl_draw_multiline_text(struct imgtl_image *image,int x,int y,int w,int h,const char *src,int srcc,uint32_t rgba) {
  if (!image) return -1;
  if (!src) return 0;
  if (!(rgba&0xff)) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int spaceminw=0;
  imgtl_measure_text(&spaceminw,0,0," ",1);
  if (spaceminw<1) spaceminw=imgtl_text.size>>1;
  int srcp=0,py=y+imgtl_text.high_ascender;
  while (srcp<srcc) {
  
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    const char *line=src+srcp;
    int linec=0,linew=0,printw=0,spacec=-1;

    // Compose each line by measuring words.
    while (srcp<srcc) {
      if ((src[srcp]==0x0a)||(src[srcp]==0x0d)) break;
      if ((unsigned char)src[srcp]<=0x20) { srcp++; linec++; continue; }
      const char *word=src+srcp;
      int wordc=0;
      while ((srcp+wordc<srcc)&&((unsigned char)src[srcp+wordc]>0x20)) wordc++;
      int wordw=0;
      if (imgtl_measure_text(&wordw,0,0,word,wordc)<0) return -1;
      int nw=linew+wordw+(linew?spaceminw:0);
      if ((nw>w)&&linew) break;
      printw+=wordw;
      linew=nw;
      linec+=wordc;
      spacec++;
      srcp+=wordc;
    }
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;

    // Divide the space evenly among all word breaks in line.
    // If there are no word breaks, whatever. Align to left edge.
    // Also, where a line is explicit broken or end of content, do not space it out.
    int spaceeach=spaceminw,spaceextra=0;
    int explicitbreak=0;
    if (srcp>=srcc) explicitbreak=1;
    if (!explicitbreak) {
      if (spacec>0) {
        int spacew=w-printw;
        spaceeach=spacew/spacec;
        spaceextra=spacew%spacec;
      }
    }

    // Iterate over the line again, this time rendering it.
    int px=x,linep=0;
    while (linep<linec) {
      if ((unsigned char)line[linep]<=0x20) { linep++; continue; }
      const char *word=line+linep;
      int wordc=0;
      while ((linep<linec)&&((unsigned char)line[linep]>0x20)) { linep++; wordc++; }
      int wordw=imgtl_draw_text(image,px,py,-1,word,wordc,rgba);
      px+=wordw+spaceeach;
      if (spaceextra) { px++; spaceextra--; }
    }

    py+=imgtl_text.size;
  }
  return 0;
}
