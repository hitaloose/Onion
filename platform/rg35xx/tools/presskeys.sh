#!/bin/sh
# Presses buttons on a running RG35XX over ADB, for testing screens without
# touching the device. Events go to padmapd's uinput keyboard, so programs see
# them like real buttons (with the Miyoo keycodes).
#
# Usage: presskeys.sh KEY...
#   KEY: up down left right a b x y l1 r1 l2 r2 select start menu power
#   (keymon sees menu and power too: GameSwitcher, suspend, shutdown)
set -e

cmd=""
for key in "$@"; do
    case $key in
    up) code=103 ;;
    down) code=108 ;;
    left) code=105 ;;
    right) code=106 ;;
    a) code=57 ;;
    b) code=29 ;;
    x) code=42 ;;
    y) code=56 ;;
    l1) code=18 ;;
    r1) code=20 ;;
    l2) code=15 ;;
    r2) code=14 ;;
    select) code=97 ;;
    start) code=28 ;;
    menu) code=1 ;;
    power) code=116 ;;
    *)
        sed -n '2,9p' "$0"
        exit 1
        ;;
    esac
    cmd="$cmd press $code;"
done

# sendevent writes raw events, the SYN after each key makes them reach SDL
adb shell "
    press() {
        sendevent /dev/input/onion-kbd 1 \$1 1; sendevent /dev/input/onion-kbd 0 0 0
        sleep 0.15
        sendevent /dev/input/onion-kbd 1 \$1 0; sendevent /dev/input/onion-kbd 0 0 0
        sleep 0.3
    }
    $cmd"
