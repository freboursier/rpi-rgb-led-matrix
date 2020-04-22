#reminder: re-enable -03
CXXFLAGS=-Wall -O3 -Wextra -Wno-unused-parameter -D_FILE_OFFSET_BITS=64 -Wno-unused-function
OBJECTS=led-image-viewer.o
BINARIES=led-image-viewer

OPTIONAL_OBJECTS=video-viewer.o emoji-scroller.o dmd-slideshow-utils.o dmd-slideshow.o Sequence.o IRRemote.o FileCollection.o LoadedFile.o
OPTIONAL_BINARIES=video-viewer emoji-scroller dmd-slideshow

# Where our library resides. You mostly only need to change the
# RGB_LIB_DISTRIBUTION, this is where the library is checked out.
RGB_LIB_DISTRIBUTION=..
RGB_INCDIR=$(RGB_LIB_DISTRIBUTION)/include
RGB_LIBDIR=$(RGB_LIB_DISTRIBUTION)/lib
RGB_LIBRARY_NAME=rgbmatrix
RGB_LIBRARY=$(RGB_LIBDIR)/lib$(RGB_LIBRARY_NAME).a
LDFLAGS+=-L$(RGB_LIBDIR) -l$(RGB_LIBRARY_NAME) -lrt -lm -lpthread
HEXEMBED=./hexembed
# Imagemagic flags, only needed if actually compiled.
#MAGICK_CXXFLAGS?=`GraphicsMagick++-config --cppflags --cxxflags`
#MAGICK_LDFLAGS?=`GraphicsMagick++-config --ldflags --libs`
MAGICK_CXXFLAGS?=`GraphicsMagickWand-config --cppflags `
MAGICK_LDFLAGS?=`GraphicsMagickWand-config --ldflags --libs`


AV_CXXFLAGS=`pkg-config --cflags  libavcodec libavformat libswscale libavutil`

all : $(BINARIES)

$(RGB_LIBRARY): FORCE
	$(MAKE) -C $(RGB_LIBDIR)

hexembed: hexembed.o
	gcc hexembed.o -o $@
	$(HEXEMBED) ../fonts/6x10.bdf smallFontHex > smallFont.c; 
	$(HEXEMBED) ../fonts/9x18B.bdf largeFontHex > largeFont.c; 
	$(HEXEMBED) ../fonts/4x6.bdf smallestFontHex > smallestFont.c; 

	

led-image-viewer: led-image-viewer.o $(RGB_LIBRARY)
	$(CXX) $(CXXFLAGS) led-image-viewer.o -o $@ $(LDFLAGS) $(MAGICK_LDFLAGS)

dmd-slideshow: hexembed dmd-slideshow-utils.o dmd-slideshow.o Sequence.o IRRemote.o  FileCollection.o LoadedFile.o $(RGB_LIBRARY)
	$(CXX) $(CXXFLAGS) dmd-slideshow.o dmd-slideshow-utils.o Sequence.o IRRemote.o  FileCollection.o LoadedFile.o -o  $@ $(LDFLAGS) $(MAGICK_LDFLAGS)


emoji-scroller: emoji-scroller.o $(RGB_LIBRARY)
	$(CXX) $(CXXFLAGS) emoji-scroller.o -o $@ $(LDFLAGS) $(MAGICK_LDFLAGS)


video-viewer: video-viewer.o $(RGB_LIBRARY)
	$(CXX) $(CXXFLAGS) video-viewer.o -o $@ $(LDFLAGS) `pkg-config --cflags --libs  libavcodec libavformat libswscale libavutil`

%.o : %.c
	gcc -c -o $@ $<


%.o : %.cc
	$(CXX) -I$(RGB_INCDIR) $(CXXFLAGS) $(MAGICK_CXXFLAGS) -c -o $@ $<

led-image-viewer.o : led-image-viewer.cc
	$(CXX) -I$(RGB_INCDIR) $(CXXFLAGS) $(MAGICK_CXXFLAGS) -c -o $@ $<

emoji-scroller.o : emoji-scroller.cc
	$(CXX) -I$(RGB_INCDIR) $(CXXFLAGS) $(MAGICK_CXXFLAGS) -c -o $@ $<
	
# We're using a couple of deprecated functions. Pull request to update this to
# the latest libraries is welcome.
video-viewer.o: video-viewer.cc
	$(CXX) -I$(RGB_INCDIR) $(CXXFLAGS) -Wno-deprecated-declarations -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(BINARIES) $(OPTIONAL_OBJECTS) $(OPTIONAL_BINARIES)

FORCE:
.PHONY: FORCE
