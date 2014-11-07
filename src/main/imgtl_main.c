#include "imgtl.h"
#include "misc/imgtl_fs.h"
#include "misc/imgtl_buffer.h"
#include "serial/imgtl_png.h"
#include "image/imgtl_image.h"
#include "draw/imgtl_draw.h"
#include "data/imgtl_deck.h"

/* Main.
 */

int main(int argc,char **argv,char **envv) {

  //imgtl_image_save_hint=IMGTL_IMAGE_SAVE_HINT_DEFAULT; // 618230 B in 3.164 s
  imgtl_image_save_hint=IMGTL_IMAGE_SAVE_HINT_FASTER; // 680506 B in 0.824 s
  //imgtl_image_save_hint=IMGTL_IMAGE_SAVE_HINT_MUCH_FASTER; // 24,020860 B in 0.224 s

  if (imgtl_deck_load("spreadsheets/tsv.tsv")<0) {
    fprintf(stderr,"Failed to acquire spreadsheet.\n");
    return 1;
  }

  if (imgtl_deck_load_background_image("spreadsheets/bg.png")<0) {
    fprintf(stderr,"Failed to open background image.\n");
    return 1;
  }

  if (imgtl_deck_begin_rendering()<0) {
    fprintf(stderr,"Failed to generate card images. (Out of memory)\n");
    return 1;
  }

  imgtl_deck_add_image("spreadsheets/Equipment/{Name}.png",-1,250,74,240,320);
  if (imgtl_text_setup("/opt/X11/share/fonts/TTF/VeraBd.ttf",48)<0) { fprintf(stderr,"Failed to acquire font.\n"); return 1; }
  imgtl_deck_add_label(imgtl_deck_lookup_field("Name",-1),250,57,0,0xffffffff);
  if (imgtl_text_setup("/opt/X11/share/fonts/TTF/Vera.ttf",24)<0) { fprintf(stderr,"Failed to acquire font.\n"); return 1; }
  imgtl_deck_add_label(imgtl_deck_lookup_field("Needed to Complete",-1),240,100,1,0x000000ff);
  imgtl_deck_add_label(imgtl_deck_lookup_field("Scrap Value",-1),240,125,1,0x000000ff);
  //imgtl_deck_add_label(imgtl_deck_lookup_field("Extra Text",-1),20,420,-1,0x808080ff);
  imgtl_deck_add_text(imgtl_deck_lookup_field("Extra Text",-1),10,394,480,406,0x808080ff);

  if (imgtl_deck_save_images("output")<0) {
    fprintf(stderr,"Failed to save images.\n");
    return 1;
  }

  imgtl_text_quit();
  imgtl_deck_quit();
  return 0;
}
