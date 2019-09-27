
#include <dirent.h>

bool    isValidDirent(struct dirent *entry);
bool    isValidImageFilename(char *filename);
const char *get_filename_ext(char *filename);
