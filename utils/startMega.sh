#!/bin/sh
set -x
DMD_SLIDESHOW="/home/pi/rpi-rgb-led-matrix/utils/dmd-slideshow"
DMD_CONFIG="--led-gpio-mapping=adafruit-hat-pwm   --led-chain=8 --led-pixel-mapper=U-mapper  --led-cols=64 --led-slowdown-gpio=4 --led-no-drop-privs"

sudo ir-keytable -c -w  /home/pi/17buttons-remote.conf
#sudo  ${DMD_SLIDESHOW} ${DMD_CONFIG}  -d /home/pi/gif -s "SONIC" -w 0 -f '.*sonic.*.gif' -w 0 -f '.*sonic.*'
#sudo  ${DMD_SLIDESHOW} ${DMD_CONFIG}  -d /home/pi/gif  -s "PINBALL" -w 0 -f 'PINBALL_STORY.*.gif' -w 3 -c '.*sonic.*.gif' -s "SONIC" -w 4 -c '.*sonic.*.gif' -w 6 -f '.*sonic.*'

#5 sec arcade solo + 5 sec sonic--0
#sudo  ${DMD_SLIDESHOW} ${DMD_CONFIG}  -d /home/pi/gif  -s "PINBALL" -w 3600 -f MD_SonicRun01.gif -w 3600 -f MD_SonicRun01.gif320
#sudo  ${DMD_SLIDESHOW} ${DMD_CONFIG}  -d /home/pi/gif  -s "PINBALL" -w 0 -f 'PINBALL_STORY.*.gif' -s "SONIC" -w 4 -c '.*sonic.*.gif' -w 6 -f '.*sonic.*' -s "Metal Slug" -w 6 -f ".*MetalSlug.*"
sudo  ${DMD_SLIDESHOW} ${DMD_CONFIG}\
  --directory /home/pi/gif \
      --sequence "PINBALL" --wait 0 --full 'PINBALL_STORY.*.gif' \
      --sequence "DBZ" --wait 6 --full ".*DBZ.*" \
      --sequence "SONIC" --wait 4 --cross '.*sonic.*.gif' --wait 6 --full '.*sonic.*' \
      --sequence "Metal Slug" --wait 6 --full ".*MetalSlug.*"

#sudo  ${DMD_SLIDESHOW} ${DMD_CONFIG}  -d /home/pi/gif  -s "PINBALL" -w 2 -f 'GBA_Rockman.gif' -w 5 -f 'ATARIST_GobliiinsI01.gif'  -s "SONIC" -w 4 -c '.*sonic.*.gif' -w 6 -f '.*sonic.*'

 #    --sequence "Adult" --wait 0 --full 'XXX.*.gif' \
