#ifndef DMD_SLIDESHOW_H_
#define DMD_SLIDESHOW_H_

#include <vector>
#include <stdio.h>
#include <Magick++.h>
#include <magick/image.h>
#include "dmd-slideshow-utils.hh"



#define debug_print(fmt, ...) \
            do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)
				


static const tmillis_t distant_future = (1LL<<40); // that is a while.

struct    LoadedFile {
    LoadedFile(): is_multi_frame(0), currentFrameID(0), nextFrameTime(-distant_future){};
    const char *filename;
    std::vector<Magick::Image> frames;
    bool is_multi_frame;
    unsigned int    currentFrameID;
    unsigned int frameCount;
    tmillis_t    nextFrameTime;
};



struct	FileCollection {
	FileCollection() : screenMode(FullScreen) , displayDuration(10){}
	const char *regex;
	ScreenMode	screenMode;
	unsigned 	int	displayDuration;
    tmillis_t   displayStartTime; // When did we start to show this collection ?
	std::vector<const char *>filePaths;
	std::vector<LoadedFile *> loadedFiles;
    unsigned short visibleImages; // Number of files show simultaneously
};

void scheduleInfoMessage();

#endif