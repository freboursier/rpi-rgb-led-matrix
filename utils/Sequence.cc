#include "Sequence.hh"
#include <regex.h>

using std::vector;

Sequence::Sequence() {}

Sequence::~Sequence() {}

void Sequence::loadCollections(vector<const char *> filenames) {
  fprintf(stderr, "Attempt to load sequence %s", name);

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
#ifdef MEGA_VERBOSE
        fprintf(stderr, "Match %s => %s\n", collection->regex, filename);
#endif
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

void Sequence::printContent() {
  fprintf(stderr, "Sequence %s / %d collections\n", name, collections.size());
  for (auto &collection : collections) {
    const char *displayMode =
        collection->screenMode == FullScreen ? "Full screen" : "Cross";
    fprintf(stderr,
            "\tCollection %s => %d images / %d seconds / display mode %s\n",
            collection->regex, collection->filePaths.size(),
            collection->displayDuration, displayMode);
  }
}

FileCollection *Sequence::nextCollection() {
  return collections[nextCollectionIdx];
}

void Sequence::forwardCollection() {
  if (currentCollection() != NULL) {
      fprintf(stderr, "About to DELETE %d loaded files\n", currentCollection()->loadedFiles.size());
    for (auto &loadedFile : currentCollection()->loadedFiles) {
    //    loadedFile->frames.clear();
        delete loadedFile;
    }
    fprintf(stderr, "About to clear %d loaded files", currentCollection()->loadedFiles.size());
    currentCollection()->loadedFiles.clear();
  }
  
  currentCollectionIdx = nextCollectionIdx;
  nextCollectionIdx = (nextCollectionIdx + 1) % collections.size();
  fprintf(stderr, "FWD, new collection is %s\n", currentCollection()->regex);
}
