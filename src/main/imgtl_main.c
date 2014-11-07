#include "imgtl.h"
#include "misc/imgtl_fs.h"
#include "misc/imgtl_buffer.h"
#include "serial/imgtl_png.h"
#include "image/imgtl_image.h"
#include "draw/imgtl_draw.h"
#include "data/imgtl_deck.h"
#include "data/imgtl_script.h"
#include <unistd.h>
#include <errno.h>

/* --help and --version
 */

static void imgtl_print_help(const char *name) {
  printf(
    "Usage: %s [SCRIPTS...]\n"
  ,name);
}

static void imgtl_print_version() {
  printf("imgtl 20141107\n");
}

/* Main.
 */

int main(int argc,char **argv,char **envv) {

  char *wd=getcwd(0,0);

  int i; for (i=1;i<argc;) {
    const char *arg=argv[i++];
    if (!strcmp(arg,"--help")) imgtl_print_help(argv[0]);
    else if (!strcmp(arg,"--version")) imgtl_print_version();
    else if (arg[0]=='-') { fprintf(stderr,"Unexpected argument '%s'.\n",arg); return 1; }
    else {
      char *src=0;
      int srcc=imgtl_file_read(&src,arg);
      if (srcc<0) {
        fprintf(stderr,"%s: Failed to read file: %s\n",arg,strerror(errno));
        return 1;
      }
      char d[1024];
      int pathc=0,slashp=-1;
      while (arg[pathc]) {
        if (arg[pathc]=='/') slashp=pathc;
        pathc++;
      }
      if ((slashp>0)&&(slashp<sizeof(d))) {
        memcpy(d,arg,slashp);
        d[slashp]=0;
        chdir(d);
      }
      if (imgtl_script_execute_text(src,srcc,arg)<0) return 1;
      if (wd) chdir(wd);
      free(src);
    }
  }

  //imgtl_image_save_hint=IMGTL_IMAGE_SAVE_HINT_DEFAULT; // 618230 B in 3.164 s
  //imgtl_image_save_hint=IMGTL_IMAGE_SAVE_HINT_FASTER; // 680506 B in 0.824 s
  //imgtl_image_save_hint=IMGTL_IMAGE_SAVE_HINT_MUCH_FASTER; // 24,020860 B in 0.224 s

  imgtl_text_quit();
  imgtl_deck_quit();
  if (wd) free(wd);
  return 0;
}
