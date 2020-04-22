
#ifndef SEQUENCE_HH_
#define SEQUENCE_HH_

#include <sys/time.h>
#include <sys/types.h>

#include "FileCollection.hh"
#include "dmd-slideshow.hh"

class Sequence {

public:
  Sequence(const char *newName, bool newTransient);
  Sequence(const char *newName);
  ~Sequence();

  FileCollection *currentCollection();
  FileCollection *nextCollection();

  void printContent();
  void loadCollections(std::vector<const char *> filenames);
  void forwardCollection();

  std::vector<FileCollection *> collections;
  int displayTime; // Display time in seconds

  unsigned int nextCollectionTargetSize();
  bool nextCollectionIsReady();
  void reset();
  const char *name();
  bool  transient;

private:
  unsigned int nextCollectionIdx = 0;
  int currentCollectionIdx = -1;
  const char *_name;
  char const*stringForScreenMode(ScreenMode screenMode);
};

#endif