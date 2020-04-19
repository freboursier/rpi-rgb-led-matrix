#include "FileCollection.hh"

FileCollection::FileCollection(ScreenMode newScreenMode, unsigned int newDisplayDuration, const char *newRegex) {
  screenMode = newScreenMode;
  if (screenMode == Cross) {
    visibleImages = 4;
  } else {
    visibleImages = 1;
  }
  loadedFiles = std::vector<LoadedFile *>();
  displayDuration = newDisplayDuration;
  regex = newRegex;

}

FileCollection::~FileCollection() {
  filePaths.clear();
  loadedFiles.clear();
}