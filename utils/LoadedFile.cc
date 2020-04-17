#include "LoadedFile.hh"

LoadedFile::LoadedFile() {
  nextFrameTime = -distant_future;
}

LoadedFile::~LoadedFile() {
  DestroyMagickWand(wand);
}