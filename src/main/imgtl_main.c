#include "imgtl.h"
#include "misc/imgtl_fs.h"
#include "misc/imgtl_buffer.h"
#include "serial/imgtl_png.h"
#include "image/imgtl_image.h"
#include "draw/imgtl_draw.h"
#include "data/imgtl_deck.h"
#include "data/imgtl_script.h"

/* Main.
 */

int main(int argc,char **argv,char **envv) {

  if (imgtl_script_execute("spreadsheets/script")<0) return 1;

  //imgtl_image_save_hint=IMGTL_IMAGE_SAVE_HINT_DEFAULT; // 618230 B in 3.164 s
  //imgtl_image_save_hint=IMGTL_IMAGE_SAVE_HINT_FASTER; // 680506 B in 0.824 s
  //imgtl_image_save_hint=IMGTL_IMAGE_SAVE_HINT_MUCH_FASTER; // 24,020860 B in 0.224 s

  imgtl_text_quit();
  imgtl_deck_quit();
  return 0;
}
