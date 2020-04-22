#include "LoadedFile.hh"
#include "dmd-slideshow.hh"

LoadedFile::LoadedFile(const char *filename, MagickWand *coalescedWand) {
  nextFrameTime = GetTimeInMillis();
  _filename = filename;
  _wand = coalescedWand;
  MagickResetIterator(_wand);
}

LoadedFile::~LoadedFile() {
  DestroyMagickWand(_wand);
  _wand = NULL;
  _filename = NULL;
}

MagickWand  *LoadedFile::wand() {
  return _wand;
}

const char *LoadedFile::filename() {
  return _filename;
}