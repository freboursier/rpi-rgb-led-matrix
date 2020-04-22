#include "Sequence.hh"
#include <regex.h>

using std::vector;

Sequence::Sequence(const char *newName) {
  _name = newName;
  transient = false;
}

Sequence::Sequence(const char *newName, bool newTransient) {
  _name = newName;
  transient = newTransient;
}

Sequence::~Sequence() {}

void Sequence::loadCollections(vector<const char *> filenames) {
  fprintf(stderr, "Attempt to load sequence %s", _name);

  for (auto &collection : collections) {
    fprintf(stderr, "Fill collection %s\n", collection->regex);

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
    fprintf(stderr, "\t%d pictures\n", collection->filePaths.size());
    regfree(&regex);
  }
}

FileCollection *Sequence::currentCollection() {
  if (currentCollectionIdx == -1) {
    return NULL;
  }
  return collections[currentCollectionIdx];
}

char const*Sequence::stringForScreenMode(ScreenMode screenMode)
{
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
  fprintf(stderr, "Sequence %s / %d collections\n", _name, collections.size());
  for (auto &collection : collections) {
    char const *displayMode = stringForScreenMode(collection->screenMode);
    fprintf(stderr, "\tCollection %s => %d images / %d seconds / display mode %s\n", collection->regex, collection->filePaths.size(), collection->displayDuration, displayMode);
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
    fprintf(stderr, "About to DELETE %d loaded files\n", current->visibleImages);
    for (int i = 0; i < current->visibleImages; i++) {
      delete current->loadedFiles[i];
    }
    current->loadedFiles.erase(current->loadedFiles.begin(), current->loadedFiles.begin() + current->visibleImages);
  }

  currentCollectionIdx = nextCollectionIdx;
  nextCollectionIdx = (nextCollectionIdx + 1) % collections.size();
  fprintf(stderr, "FWD, new collection is %s\n", currentCollection()->regex);
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
