#include "Sequence.hh"
#include <regex.h>
#include "dmd-slideshow-utils.hh"

using std::vector;

Sequence::Sequence(const char *newName) {
  _name = newName;
  transient = false;
}

Sequence::Sequence(const char *newName, bool newTransient) {
  _name = newName;
  transient = newTransient;
  _displayStartTime = GetTimeInMillis();
}

Sequence::~Sequence() {}

void Sequence::loadCollections(vector<const char *> filenames) {
  for (auto &collection : collections) {

    regex_t regex;

    int reti = regcomp(&regex, collection->regex, REG_ICASE);
    if (reti) {
      fprintf(stderr, "Could not compile regex\n");
    }

    for (auto &filename : filenames) {
      reti = regexec(&regex, filename, 0, NULL, 0);
      if (!reti) {
        collection->filePaths.push_back(filename);
      }
    }
    if (collection->filePaths.size() == 0) {
      fprintf(stderr, "A collection (%s/%s) cannot be empty\n", _name, collection->regex);
      exit(1);
    }
    regfree(&regex);
  }
}

FileCollection *Sequence::currentCollection() {
  if (currentCollectionIdx == -1) {
    return NULL;
  }
  return collections[currentCollectionIdx];
}

char const *Sequence::stringForScreenMode(ScreenMode screenMode) {
  switch (screenMode) {
  case FullScreen:
    return "Full screen";
  case Cross:
    return "Cross";
  case Splash:
    return "Splash";
  default:
    return "Unknown";
  }
}

void Sequence::printContent() {
  fprintf(stderr, "Sequence %s / %d collection(s)\n", _name, collections.size());
  for (auto &collection : collections) {
    char const *displayMode = stringForScreenMode(collection->screenMode);
    fprintf(stderr, "\tCollection %s => %d images / %d second(s) / %s\n", collection->regex, collection->filePaths.size(), collection->displayDuration, displayMode);
  }
}

FileCollection *Sequence::nextCollection() {
  return collections[nextCollectionIdx];
}

void Sequence::reset() {
  fprintf(stderr, "Reset sequence %s\n", _name);
  currentCollectionIdx = -1;
  nextCollectionIdx = 0;
  for (auto &collection : collections) {
    for (auto &loadedFile : collection->loadedFiles) {
      delete loadedFile;
    }
    collection->loadedFiles.clear();
  }
}

void Sequence::forwardCollection() {
  FileCollection *current = currentCollection();
  if (current != NULL) {
    for (int i = 0; i < current->visibleImages; i++) {
      delete current->loadedFiles[i];
    }
    current->loadedFiles.erase(current->loadedFiles.begin(), current->loadedFiles.begin() + current->visibleImages);
  } else {
    _displayStartTime = GetTimeInMillis();
  }

  currentCollectionIdx = nextCollectionIdx;
  nextCollectionIdx = (nextCollectionIdx + 1) % collections.size();
}

// The next collection should contains this many LoadedFile to be shown
unsigned int Sequence::nextCollectionTargetSize() {
  if (collections.size() == 1) {
    return nextCollection()->visibleImages * 2;
  }
  return nextCollection()->visibleImages;
}

bool Sequence::nextCollectionIsReady() {
  return nextCollectionTargetSize() == nextCollection()->loadedFiles.size();
}
const char *Sequence::name() {
  return _name;
}

void Sequence::setDisplayTime(int newDisplayTime) {
  _displayTime = newDisplayTime;
}

int Sequence::displayTime() {
  return _displayTime;
}

void Sequence::setDisplayStartTime(tmillis_t newStartTime) {
  _displayStartTime = newStartTime;
}

tmillis_t Sequence::displayStartTime() {
  return _displayStartTime;
}

bool  Sequence::isExpired()
{
  return (_displayStartTime + _displayTime * 1000 < GetTimeInMillis());
}