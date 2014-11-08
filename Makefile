all:
.SILENT:
PRECMD=echo "  $(@F)" ; mkdir -p $(@D) ;

#------------------------------------------------------------------------------

# If you get problems linking like "libfreetype.a not found", use the first LDFT instead.
CCFT:=$(shell freetype-config --cflags)
#LDFT:=$(shell freetype-config --libs)
LDFT:=/opt/local/lib/libfreetype.a

VERSION:=-DIMGTL_VERSION_NUMBER="\"$(shell date +%Y%m%d)\""
CCWARN:=-Werror -Wimplicit -Wformat -Wno-parentheses -Wno-comment -ferror-limit=1
CC:=gcc -c -MMD -O2 -Isrc $(CCWARN) $(CCFT) $(VERSION)
LD:=gcc $(LDFT)
LDPOST:=-lz -lbz2

#------------------------------------------------------------------------------

MIDDIR:=mid
OUTDIR:=out

CFILES:=$(shell find src -name '*.c')
OFILES:=$(patsubst src/%.c,$(MIDDIR)/%.o,$(CFILES))
-include $(OFILES:.o=.d)

$(MIDDIR)/%.o:src/%.c;$(PRECMD) $(CC) -o $@ $<

EXE:=$(OUTDIR)/imgtl
all:$(EXE)
$(EXE):$(OFILES);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)

#------------------------------------------------------------------------------

test:$(EXE);rm -rf output output.argv && $(EXE) -ooutput.argv spreadsheets/script && open -a /Applications/Preview.app output.argv

clean:;rm -rf mid out
