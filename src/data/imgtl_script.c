#include "imgtl.h"
#include "misc/imgtl_fs.h"
#include "misc/imgtl_text.h"
#include "image/imgtl_image.h"
#include "draw/imgtl_draw.h"
#include "imgtl_deck.h"
#include "imgtl_script.h"
#include <errno.h>

// Ensure that no valid command can have more tokens than this:
#define IMGTL_SCRIPT_TOKEN_LIMIT 8

/* Execute single instruction.
 */
 
int imgtl_script_execute_line(const char *src,int srcc,const char *refname,int lineno) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!refname) refname="<unknown>";

  // Split into tokens.
  struct token { const char *v; int c,n; } tokenv[IMGTL_SCRIPT_TOKEN_LIMIT]={0};
  int tokenc=0,srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if (tokenc>=IMGTL_SCRIPT_TOKEN_LIMIT) {
      fprintf(stderr,"%s:%d:ERROR: Too many tokens on line.\n",refname,lineno);
      return -2;
    }
    tokenv[tokenc].v=src+srcp;
    if ((src[srcp]=='"')||(src[srcp]=='\'')||((srcp<=srcc-3)&&!memcmp(src+srcp,"\xe2\x80\x9c",3))) {
      if ((tokenv[tokenc].c=imgtl_str_measure(src+srcp,srcc-srcp))<1) {
        fprintf(stderr,"%s:%d:ERROR: Unclosed quote.\n",refname,lineno);
        return -2;
      }
      srcp+=tokenv[tokenc].c;
    } else {
      tokenv[tokenc].c=0;
      while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)&&(src[srcp]!='"')&&(src[srcp]!='\'')) { srcp++; tokenv[tokenc].c++; }
    }
    tokenc++;
  }

  char sbuf[1024];
  int sbufc=0;

  // Consider first token.
  if (tokenc<1) return 0;
  #define CMD(kw,argc) \
    if ((tokenv[0].c==sizeof(kw)-1)&&!imgtl_memcasecmp(tokenv[0].v,kw,sizeof(kw)-1)) \
      if (tokenc!=(argc)+1) { \
        fprintf(stderr,"%s:%d:ERROR: Command '%.*s' expects %d arguments, have %d.\n",refname,lineno,tokenv[0].c,tokenv[0].v,argc,tokenc-1); \
        return -2; \
      } else
      
  #define CMDMULTI(kw,argclo,argchi) \
    if ((tokenv[0].c==sizeof(kw)-1)&&!imgtl_memcasecmp(tokenv[0].v,kw,sizeof(kw)-1)) \
      if ((tokenc-1<argclo)||(tokenc-1>argchi)) { \
        fprintf(stderr,"%s:%d:ERROR: Command '%.*s' expects %d..%d arguments, have %d.\n",refname,lineno,tokenv[0].c,tokenv[0].v,argclo,argchi,tokenc-1); \
        return -2; \
      } else

  // This copies all strings, to ensure termination.
  #define SARG(tokenp) \
    if (tokenp>=tokenc) return -1; \
    if ((tokenv[tokenp].v[0]=='"')||(tokenv[tokenp].v[0]=='\'')||((tokenv[tokenp].c>=3)&&!memcmp(tokenv[tokenp].v,"\xe2\x80\x9c",3))) { \
      int add=imgtl_str_eval(sbuf+sbufc,sizeof(sbuf)-sbufc,tokenv[tokenp].v,tokenv[tokenp].c); \
      if (add<0) { fprintf(stderr,"%s:%d:ERROR: Failed to parse string token.\n",refname,lineno); return -2; } \
      if (sbufc>=sizeof(sbuf)-add) { fprintf(stderr,"%s:%d:ERROR: Too much text.\n",refname,lineno); return -2; } \
      tokenv[tokenp].v=sbuf+sbufc; \
      tokenv[tokenp].c=add; \
      sbufc+=add; \
      sbuf[sbufc++]=0; \
    } else { \
      if (sbufc>=sizeof(sbuf)-tokenv[tokenp].c) { fprintf(stderr,"%s:%d:ERROR: Too much text.\n",refname,lineno); return -2; } \
      memcpy(sbuf+sbufc,tokenv[tokenp].v,tokenv[tokenp].c); \
      tokenv[tokenp].v=sbuf+sbufc; \
      sbufc+=tokenv[tokenp].c; \
      sbuf[sbufc++]=0; \
    }

  #define IARG(tokenp,lo,hi) \
    if (tokenp>=tokenc) return -1; \
    if ((tokenv[tokenp].n=imgtl_decuint_eval(tokenv[tokenp].v,tokenv[tokenp].c))<0) { \
      fprintf(stderr,"%s:%d:ERROR: Failed to evaluate '%.*s' as integer.\n",refname,lineno,tokenv[tokenp].c,tokenv[tokenp].v); \
      return -2; \
    } \
    if ((tokenv[tokenp].n<lo)||(tokenv[tokenp].n>hi)) { \
      fprintf(stderr,"%s:%d:ERROR: Integer %d out of range (%d..%d)\n",refname,lineno,tokenv[tokenp].n,lo,hi); \
      return -2; \
    }

  #define COLORARG(tokenp) \
    SARG(tokenp) \
    if (imgtl_rgba_eval(&tokenv[tokenp].n,tokenv[tokenp].v,tokenv[tokenp].c)<0) { \
      if ((tokenv[tokenp].n=imgtl_deck_lookup_field(tokenv[tokenp].v,tokenv[tokenp].c))>=0) { \
        tokenv[tokenp].n=0x00fe0100|(tokenv[tokenp].n<<24); \
      } else { \
        fprintf(stderr,"%s:%d:ERROR: Failed to evaluate '%.*s' as color.\n",refname,lineno,tokenv[tokenp].c,tokenv[tokenp].v); \
        return -2; \
      } \
    }

  #define ALIGNARG(tokenp) \
    if (tokenp>=tokenc) return -1; \
    if ((tokenv[tokenp].c==4)&&!imgtl_memcasecmp(tokenv[tokenp].v,"LEFT",4)) tokenv[tokenp].n=-1; \
    else if ((tokenv[tokenp].c==6)&&!imgtl_memcasecmp(tokenv[tokenp].v,"CENTER",6)) tokenv[tokenp].n=0; \
    else if ((tokenv[tokenp].c==6)&&!imgtl_memcasecmp(tokenv[tokenp].v,"MIDDLE",6)) tokenv[tokenp].n=0; \
    else if ((tokenv[tokenp].c==5)&&!imgtl_memcasecmp(tokenv[tokenp].v,"RIGHT",5)) tokenv[tokenp].n=1; \
    else { \
      fprintf(stderr,"%s:%d:ERROR: Expected 'left', 'center', or 'right' before '%.*s'.\n",refname,lineno,tokenv[tokenp].c,tokenv[tokenp].v); \
      return -2; \
    }

  CMD("save_hint",1) {
    SARG(1)
    if ((tokenv[1].c==5)&&!imgtl_memcasecmp(tokenv[1].v,"small",5)) imgtl_image_save_hint=IMGTL_IMAGE_SAVE_HINT_DEFAULT;
    else if ((tokenv[1].c==6)&&!imgtl_memcasecmp(tokenv[1].v,"medium",6)) imgtl_image_save_hint=IMGTL_IMAGE_SAVE_HINT_FASTER;
    else if ((tokenv[1].c==4)&&!imgtl_memcasecmp(tokenv[1].v,"fast",4)) imgtl_image_save_hint=IMGTL_IMAGE_SAVE_HINT_MUCH_FASTER;
    else {
      fprintf(stderr,"%s:%d:ERROR: save_hint must be one of: 'small', 'medium', 'fast'.\n",refname,lineno);
      return -2;
    }
    return 0;
  }

  CMD("font",2) {
    SARG(1)
    IARG(2,1,512)
    if (imgtl_text_setup(tokenv[1].v,tokenv[2].n)<0) {
      fprintf(stderr,"%s:%d:ERROR: Failed to set font '%.*s' size=%d.\n",refname,lineno,tokenv[1].c,tokenv[1].v,tokenv[2].n);
      return -2;
    }
    return 0;
  }
  
  CMD("load",1) {
    SARG(1)
    if (imgtl_deck_load(tokenv[1].v)<0) {
      fprintf(stderr,"%s:%d:ERROR: Failed to load spreadsheet '%.*s'\n",refname,lineno,tokenv[1].c,tokenv[1].v);
      return -2;
    }
    return 0;
  }
  
  CMD("background",1) {
    SARG(1)
    if (imgtl_deck_load_background_image(tokenv[1].v)<0) {
      fprintf(stderr,"%s:%d:ERROR: Failed to load background image '%.*s'\n",refname,lineno,tokenv[1].c,tokenv[1].v);
      return -2;
    }
    if (imgtl_deck_begin_rendering()<0) {
      fprintf(stderr,"%s:%d:ERROR: Failed to begin card rendering. (Likely out of memory, reduce size or count of cards).\n",refname,lineno);
      return -2;
    }
    return 0;
  }

  CMD("blank",2) {
    IARG(1,1,2048)
    IARG(2,1,2048)
    if (imgtl_deck_set_blank_background_image(tokenv[1].n,tokenv[2].n)<0) return -1;
    if (imgtl_deck_begin_rendering()<0) return -1;
    return 0;
  }

  CMD("defaultcolor",1) {
    COLORARG(1)
    if ((tokenv[1].n&0x00ffffff)==0x00fe0100) {
      fprintf(stderr,"%s:%d:ERROR: The default color can't be a field; that defeats the purpose of a default!.\n",refname,lineno);
      return -2;
    }
    imgtl_deck_default_color(tokenv[1].n);
    return 0;
  }
  
  CMD("label",5) {
    SARG(1)
    IARG(2,0,INT_MAX)
    IARG(3,0,INT_MAX)
    ALIGNARG(4)
    COLORARG(5)
    int fieldid=imgtl_deck_lookup_field(tokenv[1].v,tokenv[1].c);
    if (fieldid<0) {
      fprintf(stderr,"%s:%d:ERROR: Field '%.*s' not found.\n",refname,lineno,tokenv[1].c,tokenv[1].v);
      return -2;
    }
    if (imgtl_deck_add_label(fieldid,tokenv[2].n,tokenv[3].n,tokenv[4].n,tokenv[5].n)<0) {
      fprintf(stderr,"%s:%d:ERROR: Failed to add text.\n",refname,lineno);
      return -2;
    }
    return 0;
  }

  CMD("labelup",5) {
    SARG(1)
    IARG(2,0,INT_MAX)
    IARG(3,0,INT_MAX)
    ALIGNARG(4)
    COLORARG(5)
    int fieldid=imgtl_deck_lookup_field(tokenv[1].v,tokenv[1].c);
    if (fieldid<0) { fprintf(stderr,"%s:%d:ERROR: Field '%.*s' not found.\n",refname,lineno,tokenv[1].c,tokenv[1].v); return -2; }
    if (imgtl_deck_add_rlabel(fieldid,tokenv[2].n,tokenv[3].n,tokenv[4].n,270,tokenv[5].n)<0) {
      fprintf(stderr,"%s:%d:ERROR: Failed to add text.\n",refname,lineno);
      return -2;
    }
    return 0;
  }

  CMD("labeldown",5) {
    SARG(1)
    IARG(2,0,INT_MAX)
    IARG(3,0,INT_MAX)
    ALIGNARG(4)
    COLORARG(5)
    int fieldid=imgtl_deck_lookup_field(tokenv[1].v,tokenv[1].c);
    if (fieldid<0) { fprintf(stderr,"%s:%d:ERROR: Field '%.*s' not found.\n",refname,lineno,tokenv[1].c,tokenv[1].v); return -2; }
    if (imgtl_deck_add_rlabel(fieldid,tokenv[2].n,tokenv[3].n,tokenv[4].n,90,tokenv[5].n)<0) {
      fprintf(stderr,"%s:%d:ERROR: Failed to add text.\n",refname,lineno);
      return -2;
    }
    return 0;
  }
  
  CMD("text",6) {
    SARG(1)
    IARG(2,0,INT_MAX)
    IARG(3,0,INT_MAX)
    IARG(4,0,INT_MAX)
    IARG(5,0,INT_MAX)
    COLORARG(6)
    int fieldid=imgtl_deck_lookup_field(tokenv[1].v,tokenv[1].c);
    if (fieldid<0) {
      fprintf(stderr,"%s:%d:ERROR: Field '%.*s' not found.\n",refname,lineno,tokenv[1].c,tokenv[1].v);
      return -2;
    }
    if (imgtl_deck_add_text(fieldid,tokenv[2].n,tokenv[3].n,tokenv[4].n,tokenv[5].n,tokenv[6].n)<0) return -1;
    return 0;
  }

  CMDMULTI("labelfmt",4,6) {
    SARG(1)
    IARG(2,0,INT_MAX)
    IARG(3,0,INT_MAX)
    COLORARG(4)
    if (tokenc>=6) {
      ALIGNARG(5)
      if (tokenc>=7) {
        IARG(6,0,270)
        switch (tokenv[6].n) {
          case 0: case 90: case 180: case 270: break;
          default: fprintf(stderr,"%s:%d:ERROR: Rotation must be one of (0,90,180,270). Have %d.\n",refname,lineno,tokenv[6].n); return -2;
        }
      } else tokenv[6].n=0;
    } else tokenv[5].n=tokenv[6].n=0;
    if (imgtl_deck_add_formatted_label(tokenv[1].v,tokenv[1].c,tokenv[2].n,tokenv[3].n,tokenv[4].n,tokenv[5].n,tokenv[6].n)<0) return -1;
    return 0;
  }

  CMDMULTI("textfmt",6,7) {
    SARG(1)
    IARG(2,0,INT_MAX)
    IARG(3,0,INT_MAX)
    IARG(4,0,INT_MAX)
    IARG(5,0,INT_MAX)
    COLORARG(6)
    if (tokenc>=8) {
      IARG(7,0,270)
      switch (tokenv[7].n) {
        case 0: case 90: case 180: case 270: break;
        default: fprintf(stderr,"%s:%d:ERROR: Rotation must be one of (0,90,180,270). Have %d.\n",refname,lineno,tokenv[7].n); return -2;
      }
    } else tokenv[7].n=0;
    if (imgtl_deck_add_formatted_text(tokenv[1].v,tokenv[1].c,tokenv[2].n,tokenv[3].n,tokenv[4].n,tokenv[5].n,tokenv[6].n,tokenv[7].n)<0) return -1;
    return 0;
  }
  
  CMDMULTI("image",5,6) {
    SARG(1)
    IARG(2,0,INT_MAX)
    IARG(3,0,INT_MAX)
    IARG(4,0,INT_MAX)
    IARG(5,0,INT_MAX)
    if (tokenc>=7) { 
      IARG(6,0,270) 
      switch (tokenv[6].n) {
        case 0: case 90: case 180: case 270: break;
        default: fprintf(stderr,"%s:%d:ERROR: Rotation must be one of (0,90,180,270). Have %d.\n",refname,lineno,tokenv[6].n);
      }
    } else tokenv[6].n=0;
    if (imgtl_deck_add_image(tokenv[1].v,tokenv[1].c,tokenv[2].n,tokenv[3].n,tokenv[4].n,tokenv[5].n,tokenv[6].n)<0) {
      fprintf(stderr,"%s:%d:ERROR: Failed to add image '%.*s'.\n",refname,lineno,tokenv[1].c,tokenv[1].v);
      return -2;
    }
    return 0;
  }

  CMD("rect",5) {
    IARG(1,0,INT_MAX)
    IARG(2,0,INT_MAX)
    IARG(3,0,INT_MAX)
    IARG(4,0,INT_MAX)
    COLORARG(5)
    if (imgtl_deck_draw_rect(tokenv[1].n,tokenv[2].n,tokenv[3].n,tokenv[4].n,tokenv[5].n)<0) return -1;
    return 0;
  }

  CMD("line",5) {
    IARG(1,0,INT_MAX)
    IARG(2,0,INT_MAX)
    IARG(3,0,INT_MAX)
    IARG(4,0,INT_MAX)
    COLORARG(5)
    if (imgtl_deck_draw_line(tokenv[1].n,tokenv[2].n,tokenv[3].n,tokenv[4].n,tokenv[5].n)<0) return -1;
    return 0;
  }
  
  CMDMULTI("save",0,1) {
    const char *defpath=imgtl_deck_get_default_output_path();
    if (defpath&&defpath[0]) { // If a '-o' argument was provided, it overrides all.
      tokenv[1].v=defpath;
    } else if (tokenc>=2) { // If a path is named in the script, use it.
      SARG(1)
    } else { // No path!
      fprintf(stderr,"%s:%d:ERROR: Output path not provided. Please name it in this file, or run with '-oPATH'.\n",refname,lineno);
      return -2;
    }
    if (imgtl_deck_save_images(tokenv[1].v)<0) {
      fprintf(stderr,"%s:%d:ERROR: Save to '%s' failed: %s\n",refname,lineno,tokenv[1].v,strerror(errno));
      return -2;
    }
    return 0;
  }
  
  #undef CMD
  #undef CMDMULTI
  #undef SARG
  #undef IARG
  #undef COLORARG
  #undef ALIGNARG
  fprintf(stderr,"%s:%d:ERROR: Unknown command '%.*s'.\n",refname,lineno,tokenv[0].c,tokenv[0].v);
  return -1;
}

/* Execute loose text.
 */
 
int imgtl_script_execute_text(const char *src,int srcc,const char *refname) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!refname) refname="<unknown>";
  int srcp=0,lineno=1;
  while (srcp<srcc) {
    if ((src[srcp]==0x0a)||(src[srcp]==0x0d)) {
      if (++srcp>=srcc) break;
      if (((src[srcp]==0x0a)||(src[srcp]==0x0d))&&(src[srcp]!=src[srcp-1])) srcp++;
      lineno++;
    } else {
    
      if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
      const char *line=src+srcp;
      int linec=0,cmt=0,quote=0;
      while ((srcp<srcc)&&(src[srcp]!=0x0a)&&(src[srcp]!=0x0d)) {
        if (cmt) ;
        else if (quote) {
          if (src[srcp]==quote) quote=0;
          linec++;
        } else if ((src[srcp]=='"')||(src[srcp]=='\'')) { quote=src[srcp]; linec++; }
        else if (quote) linec++;
        else if (src[srcp]=='#') cmt=1;
        else linec++;
        srcp++;
      }
      while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
      if (!linec) continue;

      int err=imgtl_script_execute_line(line,linec,refname,lineno);
      if (err==-1) fprintf(stderr,"%s:%d: Unspecified error.\n",refname,lineno);
      if (err<0) return -1;

    }
  }
  return 0;
}

/* Read and execute file.
 */
 
int imgtl_script_execute(const char *path) {
  char *src=0;
  int srcc=imgtl_file_read(&src,path);
  if ((srcc<0)||!src) {
    fprintf(stderr,"%s: %s\n",path,strerror(errno));
    return -1;
  }
  int err=imgtl_script_execute_text(src,srcc,path);
  free(src);
  return err;
}
