#include "imgtl.h"
#include "misc/imgtl_fs.h"
#include "misc/imgtl_text.h"
#include "image/imgtl_image.h"
#include "draw/imgtl_draw.h"
#include "imgtl_deck.h"
#include <sys/stat.h>
#include <errno.h>

/* Globals.
 */

struct imgtl_header {
  char *name; int namec;
};

struct imgtl_value {
  char *v; int c;
};

struct imgtl_card {
  struct imgtl_value *valuev; int valuec,valuea; // Parallels deck's headers.
  struct imgtl_image *image;
};

static struct {
  struct imgtl_header *headerv; int headerc,headera;
  struct imgtl_card *cardv; int cardc,carda;
  struct imgtl_image *bgimg;
  uint32_t defaultcolor;
} imgtl_deck={0};

/* Minor types.
 */

static void imgtl_header_cleanup(struct imgtl_header *header) {
  if (!header) return;
  if (header->name) free(header->name);
}

static void imgtl_value_cleanup(struct imgtl_value *value) {
  if (!value) return;
  if (value->v) free(value->v);
}

static void imgtl_card_cleanup(struct imgtl_card *card) {
  if (!card) return;
  if (card->valuev) {
    while (card->valuec-->0) imgtl_value_cleanup(card->valuev+card->valuec);
    free(card->valuev);
  }
  imgtl_image_del(card->image);
}

/* Quit.
 */

void imgtl_deck_quit() {
  if (imgtl_deck.headerv) {
    while (imgtl_deck.headerc-->0) imgtl_header_cleanup(imgtl_deck.headerv+imgtl_deck.headerc);
    free(imgtl_deck.headerv);
  }
  if (imgtl_deck.cardv) {
    while (imgtl_deck.cardc-->0) imgtl_card_cleanup(imgtl_deck.cardv+imgtl_deck.cardc);
    free(imgtl_deck.cardv);
  }
  imgtl_image_del(imgtl_deck.bgimg);
  memset(&imgtl_deck,0,sizeof(imgtl_deck));
}

void imgtl_deck_unload() {
  while (imgtl_deck.headerc>0) imgtl_header_cleanup(imgtl_deck.headerv+(--(imgtl_deck.headerc)));
  while (imgtl_deck.cardc>0) imgtl_card_cleanup(imgtl_deck.cardv+(--(imgtl_deck.cardc)));
  imgtl_image_del(imgtl_deck.bgimg); imgtl_deck.bgimg=0;
}

/* Add card from TSV data.
 */

static int imgtl_deck_add_card(const char *src,int srcc,const char *refname) {

  if (imgtl_deck.cardc>=imgtl_deck.carda) {
    int na=imgtl_deck.carda+16;
    void *nv=realloc(imgtl_deck.cardv,sizeof(struct imgtl_card)*na);
    if (!nv) return -1;
    imgtl_deck.cardv=nv;
    imgtl_deck.carda=na;
  }
  struct imgtl_card *card=imgtl_deck.cardv+imgtl_deck.cardc++;
  memset(card,0,sizeof(struct imgtl_card));
  
  int srcp=0;
  while (srcp<srcc) {
    const char *v=src+srcp;
    int vc=0;
    while ((srcp<srcc)&&(src[srcp++]!=0x09)) vc++;
    while (vc&&((unsigned char)v[0]<=0x20)) { v++; vc--; }
    while (vc&&((unsigned char)v[vc-1]<=0x20)) vc--;
    
    if (card->valuec>=card->valuea) {
      int na=card->valuea+8;
      if (na>INT_MAX/sizeof(struct imgtl_value)) return -1;
      void *nv=realloc(card->valuev,sizeof(struct imgtl_value)*na);
      if (!nv) return -1;
      card->valuev=nv;
      card->valuea=na;
    }
    struct imgtl_value *value=card->valuev+card->valuec++;
    memset(value,0,sizeof(struct imgtl_value));

    if (vc) {
      if (!(value->v=malloc(vc+1))) return -1;
      memcpy(value->v,v,vc);
      value->v[vc]=0;
      value->c=vc;
    }
    
  }
  return 0;
}

/* Add headers from TSV data.
 */

static int imgtl_deck_add_headers(const char *src,int srcc,const char *refname) {
  int srcp=0;
  while (srcp<srcc) {
    const char *name=src+srcp;
    int namec=0;
    while ((srcp<srcc)&&(src[srcp++]!=0x09)) namec++;
    while (namec&&((unsigned char)name[0]<=0x20)) { namec--; name++; }
    while (namec&&((unsigned char)name[namec-1]<=0x20)) namec--;
    if (imgtl_deck.headerc>=imgtl_deck.headera) {
      int na=imgtl_deck.headera+16;
      if (na>INT_MAX/sizeof(struct imgtl_header)) return -1;
      void *nv=realloc(imgtl_deck.headerv,sizeof(struct imgtl_header)*na);
      if (!nv) return -1;
      imgtl_deck.headerv=nv;
      imgtl_deck.headera=na;
    }
    struct imgtl_header *header=imgtl_deck.headerv+imgtl_deck.headerc++;
    memset(header,0,sizeof(struct imgtl_header));
    if (namec) {
      if (!(header->name=malloc(namec+1))) return -1;
      memcpy(header->name,name,namec);
      header->name[namec]=0;
      header->namec=namec;
    }
  }
  return 0;
}

/* Read spreadsheet.
 */

static int imgtl_deck_read_tsv(const char *src,int srcc,const char *refname) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0;
  while (srcp<srcc) {
    const char *line=src+srcp;
    int allspace=1;
    int linec=0; while ((srcp<srcc)&&(src[srcp]!=0x0a)) { srcp++; if (line[linec]>0x20) allspace=0; linec++; }
    while ((srcp<srcc)&&(src[srcp]==0x0a)) srcp++;
    if (allspace) continue;
    if (imgtl_deck.headerc) {
      if (imgtl_deck_add_card(line,linec,refname)<0) return -1;
    } else {
      if (imgtl_deck_add_headers(line,linec,refname)<0) return -1;
    }
  }
  return 0;
}

/* Load.
 */

int imgtl_deck_load(const char *path) {
  char *src=0;
  int srcc=imgtl_file_read(&src,path);
  if ((srcc<0)||!src) return -1;
  imgtl_deck_unload();
  imgtl_deck.defaultcolor=0x000000ff;
  if (imgtl_deck_read_tsv(src,srcc,path)<0) { free(src); return -1; }
  free(src);
  return 0;
}

/* Load background image.
 */
 
int imgtl_deck_load_background_image(const char *path) {
  struct imgtl_image *image=0;
  if (path&&path[0]) {
    if (!(image=imgtl_image_load_file(path))) return -1;
  }
  imgtl_image_del(imgtl_deck.bgimg);
  imgtl_deck.bgimg=image;
  return 0;
}

/* Card accessors.
 */

int imgtl_deck_count_cards() {
  return imgtl_deck.cardc;
}

int imgtl_deck_count_fields() {
  return imgtl_deck.headerc;
}

int imgtl_deck_default_color(uint32_t rgba) {
  imgtl_deck.defaultcolor=rgba;
  return 0;
}

/* Get field by index.
 */
 
int imgtl_deck_get_indexed_field(void *kpp,int *kc,void *vpp,int *vc,int cardid,int fieldid) {
  if ((cardid<0)||(cardid>=imgtl_deck.cardc)) return -1;
  if (fieldid<0) return -1;
  struct imgtl_card *card=imgtl_deck.cardv+cardid;
  if (fieldid>=card->valuec) {
    if (!fieldid) { // field 0 is required by all cards.
      if (kpp) *(void**)kpp=""; if (kc) *kc=0; 
      if (vpp) *(void**)vpp=""; if (vc) *vc=0; 
      return 0;
    }
    return -1;
  }
  if (fieldid<imgtl_deck.headerc) {
    if (kpp) *(void**)kpp=imgtl_deck.headerv[fieldid].name;
    if (kc) *kc=imgtl_deck.headerv[fieldid].namec;
  } else {
    if (kpp) *(void**)kpp="";
    if (kc) *kc=0;
  }
  if (vpp) *(void**)vpp=card->valuev[fieldid].v;
  if (vc) *vc=card->valuev[fieldid].c;
  int n=imgtl_decuint_eval(card->valuev[fieldid].v,card->valuev[fieldid].c);
  if (n<0) return 0;
  return n;
}

/* Get field by name.
 */
 
int imgtl_deck_lookup_field(const char *name,int namec) {
  if (!name) return -1;
  if (namec<0) { namec=0; while (name[namec]) namec++; }
  if (namec<1) return -1;
  int i; for (i=0;i<imgtl_deck.headerc;i++) {
    if (namec!=imgtl_deck.headerv[i].namec) continue;
    if (imgtl_memcasecmp(name,imgtl_deck.headerv[i].name,namec)) continue;
    return i;
  }
  return -1;
}

/* Begin rendering.
 */
 
int imgtl_deck_begin_rendering() {
  if (imgtl_deck.bgimg) {
    int i; for (i=0;i<imgtl_deck.cardc;i++) {
      struct imgtl_card *card=imgtl_deck.cardv+i;
      struct imgtl_image *image=imgtl_image_copy(-1,imgtl_deck.bgimg,0,0,-1,-1);
      if (!image) return -1;
      imgtl_image_del(card->image);
      card->image=image;
    }
  } else {
    int i; for (i=0;i<imgtl_deck.cardc;i++) {
      struct imgtl_card *card=imgtl_deck.cardv+i;
      imgtl_image_del(card->image);
      card->image=0;
    }
  }
  return 0;
}

/* Add label.
 */
 
int imgtl_deck_add_label(int fieldid,int x,int y,int align,uint32_t rgba) {
  if ((fieldid<0)||(fieldid>=imgtl_deck.headerc)) return -1;
  int colorfield=-1;
  if ((rgba&0x00ffffff)==0x00fe0100) colorfield=(rgba>>24)&0xff;
  int i; for (i=0;i<imgtl_deck.cardc;i++) {
    struct imgtl_card *card=imgtl_deck.cardv+i;
    if (fieldid>=card->valuec) continue;
    if (card->valuev[fieldid].c<1) continue;
    if (colorfield>=0) {
      rgba=imgtl_deck.defaultcolor;
      if (colorfield<card->valuec) {
        imgtl_rgba_eval((int*)&rgba,card->valuev[colorfield].v,card->valuev[colorfield].c);
      }
    }
    if (imgtl_draw_text(card->image,x,y,align,card->valuev[fieldid].v,card->valuev[fieldid].c,rgba)<0) return -1;
  }
  return 0;
}

/* Add rotated label.
 */

int imgtl_deck_add_rlabel(int fieldid,int x,int y,int align,int rotation,uint32_t rgba) {
  if ((fieldid<0)||(fieldid>=imgtl_deck.headerc)) return -1;
  int colorfield=-1;
  if ((rgba&0x00ffffff)==0x00fe0100) colorfield=(rgba>>24)&0xff;
  int i; for (i=0;i<imgtl_deck.cardc;i++) {
    struct imgtl_card *card=imgtl_deck.cardv+i;
    if (fieldid>=card->valuec) continue;
    if (card->valuev[fieldid].c<1) continue;
    if (colorfield>=0) {
      rgba=imgtl_deck.defaultcolor;
      if (colorfield<card->valuec) {
        imgtl_rgba_eval((int*)&rgba,card->valuev[colorfield].v,card->valuev[colorfield].c);
      }
    }
    if (imgtl_draw_rotated_text(card->image,x,y,align,card->valuev[fieldid].v,card->valuev[fieldid].c,rgba,rotation)<0) return -1;
  }
  return 0;
}

/* Add multiline text.
 */
 
int imgtl_deck_add_text(int fieldid,int x,int y,int w,int h,uint32_t rgba) {
  if ((fieldid<0)||(fieldid>=imgtl_deck.headerc)) return -1;
  //TODO Margins?
  x+=5; w-=10;
  y+=5; h-=10;
  int colorfield=-1;
  if ((rgba&0x00ffffff)==0x00fe0100) colorfield=(rgba>>24)&0xff;
  int i; for (i=0;i<imgtl_deck.cardc;i++) {
    struct imgtl_card *card=imgtl_deck.cardv+i;
    if (fieldid>=card->valuec) continue;
    if (card->valuev[fieldid].c<1) continue;
    if (colorfield>=0) {
      rgba=imgtl_deck.defaultcolor;
      if (colorfield<card->valuec) {
        imgtl_rgba_eval((int*)&rgba,card->valuev[colorfield].v,card->valuev[colorfield].c);
      }
    }
    if (imgtl_draw_multiline_text(card->image,x,y,w,h,card->valuev[fieldid].v,card->valuev[fieldid].c,rgba)<0) return -1;
  }
  return 0;
}

/* Add image.
 */

static int imgtl_deck_add_image_1(struct imgtl_image *dst,int x,int y,int w,int h,struct imgtl_image *src) {
  if (!dst||!src) return -1;
  int srcx=0,srcy=0;
  if ((w<1)||(h<1)) {
    x-=src->w>>1;
    y-=src->h>>1;
    w=src->w;
    h=src->h;
  } else {
    if (w<src->w) {
      int adj=(src->w-w)>>1;
      srcx+=adj;
    } else if (w>src->w) x=x+(w>>1)-(src->w>>1);
    if (h<src->h) {
      int adj=(src->h-h)>>1;
      srcy+=adj;
    } else if (h>src->h) y=y+(h>>1)-(src->h>>1);
  }
  return imgtl_draw_image(dst,x,y,src,srcx,srcy,w,h);
}
 
int imgtl_deck_add_image(const char *path,int pathc,int x,int y,int w,int h,int rotation) {

  // Parse and validate path.
  if (!path) return -1;
  if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  if (!pathc) return -1;
  const char *pfx=path,*fldname=0,*sfx=0;
  int pfxc=0,fldnamec=0,sfxc=0,pathp=0;
  for (;pathp<pathc;pathp++) {
    if (path[pathp]=='{') {
      if (fldname) return -1;
      fldname=path+pathp+1;
    } else if (path[pathp]=='}') {
      if (sfx||!fldname) return -1;
      sfx=path+pathp+1;
    } else if (sfx) {
      sfxc++;
    } else if (fldname) {
      fldnamec++;
    } else {
      pfxc++;
    }
  }
  if (fldname&&!sfx) return -1;

  // Validate rotation.
  int xform;
  switch (rotation) {
    case 0: xform=IMGTL_XFORM_NONE; break;
    case 90: xform=IMGTL_XFORM_CLOCK; break;
    case 180: xform=IMGTL_XFORM_180; break;
    case 270: xform=IMGTL_XFORM_COUNTER; break;
    default: return -1;
  }

  // Insert field content.
  if (fldname) {
    int fldp=imgtl_deck_lookup_field(fldname,fldnamec);
    if (fldp<0) return -1;
    char subpath[1024];
    int i; for (i=0;i<imgtl_deck.cardc;i++) {
      struct imgtl_card *card=imgtl_deck.cardv+i;
      if (fldp>=card->valuec) continue;
      if (!card->valuev[fldp].c) continue;
      int subpathc=snprintf(subpath,sizeof(subpath),"%.*s%.*s%.*s",pfxc,pfx,card->valuev[fldp].c,card->valuev[fldp].v,sfxc,sfx);
      if ((subpathc<0)||(subpathc>=sizeof(subpath))) return -1;
      struct imgtl_image *src=imgtl_image_load_file(subpath);
      if (!src) continue;
      if (rotation) {
        struct imgtl_image *rimg=imgtl_xform(src,xform);
        if (!rimg) return -1;
        imgtl_image_del(src);
        src=rimg;
      }
      imgtl_deck_add_image_1(card->image,x,y,w,h,src);
      imgtl_image_del(src);
    }

  // Draw one image to every card.
  } else {
    struct imgtl_image *src=imgtl_image_load_file(path);
    if (!src) return -1;
    if (rotation) {
      struct imgtl_image *rimg=imgtl_xform(src,xform);
      if (!rimg) return -1;
      imgtl_image_del(src);
      src=rimg;
    }
    int i; for (i=0;i<imgtl_deck.cardc;i++) {
      struct imgtl_card *card=imgtl_deck.cardv+i;
      imgtl_deck_add_image_1(card->image,x,y,w,h,src);
    }
    imgtl_image_del(src);

  }
  return 0;
}

/* Save images.
 */
 
int imgtl_deck_save_images(const char *path) {
  if (!path||!path[0]) return -1;
  if (mkdir(path,0775)<0) {
    if (errno!=EEXIST) return -1;
  }
  int i; for (i=0;i<imgtl_deck.cardc;i++) {
    struct imgtl_card *card=imgtl_deck.cardv+i;
    char subpath[1024];
    int subpathc=snprintf(subpath,sizeof(subpath),"%s/%03d-FRONT.png",path,i+1);
    if ((subpathc<1)||(subpathc>=sizeof(subpath))) return -1;
    if (imgtl_image_save_file(subpath,card->image)<0) return -1;
  }
  return 0;
}
