
#ifndef FILE_COLLECTION_H
#define FILE_COLLECTION_H

#include "dmd-slideshow-utils.hh"
#include "LoadedFile.hh"

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

#endif