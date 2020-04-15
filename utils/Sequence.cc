#include "Sequence.hh"

Sequence::Sequence()  {}

Sequence::~Sequence()  {}

FileCollection  *Sequence::currentCollection()
{
    if (currentCollectionIdx == -1)
    {
        return NULL;
    }
    return collections[currentCollectionIdx];
}

        void        Sequence::printContent()
        {
             fprintf(stderr, "Sequence %s / %d collections\n", name, collections.size());
        for ( auto &collection : collections ) {
            const char *displayMode = collection->screenMode == FullScreen ? "Full screen" : "Cross";
            fprintf(stderr, "\tCollection %s => %d images / %d seconds / display mode %s\n",  collection->regex, collection->filePaths.size(), collection->displayDuration,  displayMode);
        }
        }


FileCollection  *Sequence::nextCollection()
{
    return collections[nextCollectionIdx];
}

        void        Sequence::forwardCollection()
        {
             currentCollectionIdx = nextCollectionIdx;
          nextCollectionIdx = (nextCollectionIdx + 1) % collections.size();
        }
