#!/bin/sh
sudo rm -f /dev/ttyUSB3dp
socat -d -d pty,raw,echo=0,link=./tty3dpm pty,raw,echo=0,link=./tty3dps &
sudo ln -s $(pwd)/tty3dpm /dev/ttyUSB3dp
#Octoprint needs to be able to read from its tty:
sudo chmod +r ./tty3dpm ./tty3dps
#need sudo for io control. #nice --15. Negative nice values = higher priority. nice -20 is low pri, nice --20 is high pri
sudo nice --15 ./build/printipi ./tty3dps ./tty3dps $@
