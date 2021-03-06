#include "IRRemote.hh"
#include "dmd-slideshow.hh"
#include <fcntl.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) < (b)) ? (b) : (a))
#define BRIGHTNESS_INCREMENT 5
#define INFO_MESSAGE_LENGTH 30

extern char gl_infoMessage[INFO_MESSAGE_LENGTH];
extern bool next_sequence_received;
extern bool show_filename;
extern int display_status;
void *MonitorIRRemote(void *inParam) {
  int fd, rd;
  unsigned int i;
  struct input_event ev[64];
  RGBMatrix *matrix = (RGBMatrix *)inParam;
  const char *inputDevice = "/dev/input/event0";
  if ((fd = open(inputDevice, O_RDONLY)) < 0) {
    perror("Failed to open IR event source");
    return (void *)1;
  }

  while (1) {
    rd = read(fd, ev, sizeof(struct input_event) * 64);

    if (rd < (int)sizeof(struct input_event)) {
      perror("Remote: read input event size is too small");
      return (void *)1;
    }

    for (i = 0; i < rd / sizeof(struct input_event); i++) {
      if (ev[i].type == EV_KEY && (ev[i].value == 1 || ev[i].value == 2)) {
        switch (ev[i].code) {

        case KEY_0: {
          break;
        }
        case KEY_1: {
          break;
        }
        case KEY_2: {
          break;
        }
        case KEY_3: {
          break;
        }
        case KEY_4: {
          break;
        }
        case KEY_5: {
          break;
        }
        case KEY_6: {
          break;
        }
        case KEY_7: {
          break;
        }
        case KEY_8: {
          break;
        }
        case KEY_9: {
          break;
        }
        case KEY_NUMERIC_STAR: {
          break;
        }
        case KEY_NUMERIC_POUND: {
          break;
        }
        case KEY_UP: {
          if (display_status == true) {
            uint8_t newBrightness = MIN(matrix->brightness() + BRIGHTNESS_INCREMENT, 100);
            changeBrightnessLevel(matrix, newBrightness);
          }
          break;
        }
        case KEY_DOWN: {
          if (display_status == true) {
            uint8_t newBrightness = MAX(matrix->brightness() - BRIGHTNESS_INCREMENT, 0);
            changeBrightnessLevel(matrix, newBrightness);
          }
          break;
        }
        case KEY_LEFT: {
          show_filename = !show_filename;
          break;
        }
        case KEY_RIGHT: {
          if (display_status == true) {
            next_sequence_received = true;
          }
          break;
        }
        case KEY_OK: {
          display_status = (display_status == DISPLAY_ENABLED) ? DISPLAY_SHOULD_CLEAR : DISPLAY_ENABLED;
          break;
        }
        }
      }
    }
  }

  return (void *)0;
}
