#ifndef DMD_SLIDESHOW_H_
#define DMD_SLIDESHOW_H_

#include <vector>
#include <stdio.h>
#include <Magick++.h>
#include <magick/image.h>



#define debug_print(fmt, ...) \
            do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)
				
typedef int64_t tmillis_t;

static const tmillis_t distant_future = (1LL<<40); // that is a while.

struct    LoadedFile {
    LoadedFile(): is_multi_frame(0), currentFrameID(0), nextFrameTime(-distant_future){};
    const char *filename;
    std::vector<Magick::Image> frames;
    bool is_multi_frame;
    unsigned int    currentFrameID;
    unsigned int frameCount;
    //     int64_t delay_time_us;
    tmillis_t    nextFrameTime;
};

typedef enum  {
	FullScreen = 0,
	Cross = 1
  } ScreenMode;

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

// struct Sequence {
//     std::vector<FileCollection *> collections;
//     const char *name;
//     int     displayTime; // Display timee in seconds
// };


static tmillis_t GetTimeInMillis() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

static void SleepMillis(tmillis_t milli_seconds) {
    if (milli_seconds <= 0) return;
    struct timespec ts;
    ts.tv_sec = milli_seconds / 1000;
    ts.tv_nsec = (milli_seconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

#endif