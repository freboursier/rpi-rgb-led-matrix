
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

  unsigned int nextCollectionTargetSize();
  bool nextCollectionIsReady();
  void reset();
  const char *name();
  bool transient;

  void setDisplayTime(int newDisplayTime);
  int displayTime();
  void setDisplayStartTime(tmillis_t newStartTime);
  tmillis_t displayStartTime();

  bool isExpired();

private:
  unsigned int nextCollectionIdx = 0;
  int currentCollectionIdx = -1;
  const char *_name;
  char const *stringForScreenMode(ScreenMode screenMode);
  int _displayTime; // Display time in seconds
  tmillis_t _displayStartTime;
};

#endif