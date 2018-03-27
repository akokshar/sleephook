#!/bin/bash

. ~/.bash_lib

Notice "$0: Start"

Notice "$0: Switch off audio"
/usr/bin/osascript -e 'set volume 0'

/Users/${USER}/bin/DisplayBrightness -s 0.05

#UmountStorage

# switch off wifi
#/usr/sbin/networksetup -setairportpower en0 off

Notice "$0: Done"

exit 0
