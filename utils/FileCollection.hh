
#ifndef FILE_COLLECTION_H
#define FILE_COLLECTION_H

#include "LoadedFile.hh"
#include "dmd-slideshow-utils.hh"

class FileCollection {

public:
  FileCollection(ScreenMode screenMode, unsigned int displayDuration, const char *regex);
  ~FileCollection();

  const char *regex;
  ScreenMode screenMode;
  unsigned int displayDuration;
  tmillis_t _displayStartTime; // When did we start to show this collection ?
  std::vector<const char *> filePaths;
  std::vector<LoadedFile *> loadedFiles;
  unsigned short visibleImages; // Number of files show simultaneously

  void    setDisplayStartTime(tmillis_t newStartTime);
  tmillis_t displayStartTime();
};

#endif