
#ifndef SEQUENCE_HH_
#define SEQUENCE_HH_

#include <sys/time.h>
#include <sys/types.h>

#include "dmd-slideshow.h"
#include "FileCollection.hh"

class Sequence {
    public:
        Sequence();
        ~Sequence();

        FileCollection  *currentCollection();
        FileCollection  *nextCollection();

        void        printContent();
        void        loadCollections(std::vector<const char *> filenames);
        void        forwardCollection();

    std::vector<FileCollection *> collections;
    const char *name;
    int     displayTime; // Display time in seconds

    private:
          unsigned int nextCollectionIdx = 0;
          int currentCollectionIdx = -1;
};

#endif