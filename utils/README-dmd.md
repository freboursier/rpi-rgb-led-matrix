 sudo ./dmd-slideshow --led-show-refresh  --led-chain=8 --led-pixel-mapper="U-mapper"  --led-cols=64 --led-slowdown-gpio=3 --led-no-drop-privs  -d ~/gif -c '.*sonic.*' -f '.*bubble.*'

Debug:

sudo -s
 gdb --args ./dmd-slideshow   --led-chain=8 --led-pixel-mapper="U-mapper"  --led-cols=64 --led-slowdown-gpio=3 --led-no-drop-privs  -d /home/pi/gif  -w 3 -c 'PINBALL_Short.*.gif' -f 'ARCADE.*'