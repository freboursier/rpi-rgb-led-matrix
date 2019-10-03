
#include <dirent.h>
#include <stdint.h>

bool    isValidDirent(struct dirent *entry);
bool    isValidImageFilename(char *filename);
const char *get_filename_ext(char *filename);
int usage(const char *progname);
void    setThreadPriority(int priority, uint32_t affinity_mask);