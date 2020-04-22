#ifndef SLIDESHOW_TYPE_H
#define SLIDESHOW_TYPE_H

typedef int64_t tmillis_t;

typedef enum { FullScreen = 0, Cross = 1, Splash = 2} ScreenMode;

#define     DISPLAY_ENABLED          1
#define     DISPLAY_SHOULD_CLEAR 1 << 2
#define     DISPLAY_OFF     1 << 3

#endif