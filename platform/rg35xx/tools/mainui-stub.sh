#!/bin/sh
# Temporary MainUI for the RG35XX until src/mainUI exists: a prompt menu that
# speaks the runtime.sh contract (/tmp/cmd_to_run.sh, .runGameSwitcher, .offOrder).
sysdir=/mnt/SDCARD/.tmp_update

cd $sysdir
./bin/prompt -t "Onion RG35XX" -m "MainUI provisorio" \
    "GameSwitcher" "Tweaks" "Temas" "Activity Tracker" "Desligar"

case $? in
0) touch $sysdir/.runGameSwitcher ;;
1) echo '"/mnt/SDCARD/App/Tweaks/launch.sh"' > /tmp/cmd_to_run.sh ;;
2) echo '"/mnt/SDCARD/App/ThemeSwitcher/launch.sh"' > /tmp/cmd_to_run.sh ;;
3) echo '"/mnt/SDCARD/App/PlayActivity/launch.sh"' > /tmp/cmd_to_run.sh ;;
4) touch /tmp/.offOrder ;;
esac
