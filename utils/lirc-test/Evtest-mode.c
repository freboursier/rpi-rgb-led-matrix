
	#include <stdint.h>

	#include <linux/input.h>

	#include <string.h>
	#include <fcntl.h>
	#include <unistd.h>
	#include <stdio.h>

	int main (int argc, char **argv)
	{
		int fd, rd, i;/
		struct input_event ev[64];

		if (argc < 2) {
			printf("Usage: evtest /dev/input/eventX\n");
			printf("Where X = input device number\n");
			return 1;
		}

		if ((fd = open(argv[argc - 1], O_RDONLY)) < 0) {
			perror("evtest");
			return 1;
		}


		printf("Testing ... (interrupt to exit)\n");

		while (1) {
			rd = read(fd, ev, sizeof(struct input_event) * 64);

			if (rd < (int) sizeof(struct input_event)) {
				printf("yyy\n");
				perror("\nevtest: error reading");
				return 1;
			}

			for (i = 0; i < rd / sizeof(struct input_event); i++)
				{
					if (ev[i].type == EV_KEY && (ev[i].value == 1 || ev[i].value == 2)) {
							switch (ev[i].code) {

	case KEY_0:
	{
	break;
	}
	case KEY_1:
	{
	break;
	}
	case KEY_2:
	{
	break;
	}
	case KEY_3:
	{
	break;
	}
	case KEY_4:
	{
	break;
	}
	case KEY_5:
	{
	break;
	}
	case KEY_6:
	{
	break;
	}
	case KEY_7:
	{
	break;
	}
	case KEY_8:
	{
	break;
	}
	case KEY_9:
	{
	break;
	}
	case KEY_NUMERIC_STAR:
	{
	break;
	}
	case KEY_NUMERIC_POUND:
	{
	break;
	}
	case KEY_UP:
	{
	break;
	}
	case KEY_DOWN:
	{
	break;
	}
	case KEY_LEFT:
	{
	break;
	}
	case KEY_RIGHT:
	{
	break;
	}
	case KEY_OK:
	{
	printf("OKOK!\n");
	break;
	}

							}
		

						// printf("Event: time %ld.%06ld, type %d (%s), code %d (%s), value %d\n",
						// ev[i].time.tv_sec, ev[i].time.tv_usec, ev[i].type,
						// events[ev[i].type] ? events[ev[i].type] : "?",
						// ev[i].code,
						// names[ev[i].type] ? (names[ev[i].type][ev[i].code] ? names[ev[i].type][ev[i].code] : "?") : "?",
						// ev[i].value);
				}	
				}

		}
	}


