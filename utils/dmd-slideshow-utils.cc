#include "dmd-slideshow-utils.hh"
#include <string.h>

bool    isValidDirent(struct dirent *entry)
{
    if (entry->d_type != DT_REG) {
        return false;
    }
    return isValidImageFilename(entry->d_name);
}

bool    isValidImageFilename(char *filename)
{
    const char *extension = get_filename_ext(filename);
    return (strcmp(extension, "gif") == 0 || strcmp(extension, "jpg"));
}


const char *get_filename_ext(char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}
