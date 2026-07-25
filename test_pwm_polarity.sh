#!/usr/bin/env bash
#
# test_pwm_polarity.sh
#
# For IT87-family boards that hit:
#     it87 ...: Detected broken BIOS defaults, disabling PWM interface
#
# This tells you which module parameter restores working fan control:
#     force_pwm=1          (board wants active-low polarity)
#     fix_pwm_polarity=1   (board wants active-high polarity)
#     or that PWM is genuinely unusable.
#
# How it works: it loads the driver with force_pwm=1, which forces a known
# active-low state and enables the PWM interface, then drives the fan to full
# and to off and watches the tachometer. If the fan responds in the normal
# direction, active-low is correct. If it responds inverted, the board wants
# active-high, so fix_pwm_polarity is the right knob. No clear response means
# there's no usable tach on that channel or PWM can't be made to work.
#
# SAFE TO RUN: it spins the fan up and down for a few seconds and reloads the
# it87 module. It changes NO persistent configuration. If you run a fan-control
# daemon (fancontrol, or your own service), stop it first so it does not fight
# this script for the pwm, then restart it after.
#
# Boards that need extra load-time params (e.g. ignore_resource_conflict=1)
# can pass them via the IT87_PARAMS environment variable.
#
# Usage:
#   sudo ./test_pwm_polarity.sh [-y]
#   sudo IT87_PARAMS="ignore_resource_conflict=1" ./test_pwm_polarity.sh

set -uo pipefail

SETTLE=5                             # seconds to wait after a pwm change before reading the tach
THRESH=200                           # min RPM gap to call a direction with confidence
EXTRA_PARAMS="${IT87_PARAMS:-}"      # extra modprobe params for this board

die() { echo "ERROR: $*" >&2; exit 1; }
[ "$(id -u)" -eq 0 ] || die "must run as root (it loads modules and writes pwm)"

if [ "${1:-}" != "-y" ]; then
    echo "This will reload the it87 module and briefly spin your fan to full and to off."
    echo "Stop any fan-control daemon (fancontrol, custom service) before continuing."
    read -r -p "Continue? [y/N] " ans
    case "$ans" in [yY]*) ;; *) echo "aborted."; exit 0 ;; esac
fi

echo ">> Reloading it87 with force_pwm=1 (forces a known active-low state)..."
modprobe -r it87 2>/dev/null
# shellcheck disable=SC2086
modprobe it87 force_pwm=1 $EXTRA_PARAMS || die "could not load it87 with force_pwm=1"

# From here on, restore a safe state on any exit: drop force_pwm so the board's
# own hardware default takes the fan back (the safe "fan just runs" condition).
restore() {
    echo ">> Restoring fan to hardware default (reloading without force_pwm)."
    echo "   Apply your chosen parameter and reboot to make control permanent."
    modprobe -r it87 2>/dev/null
    # shellcheck disable=SC2086
    modprobe it87 $EXTRA_PARAMS 2>/dev/null || true
}
trap restore EXIT

# Locate the it87 hwmon by name.
HW=""
for d in /sys/class/hwmon/hwmon*; do
    n=$(cat "$d/name" 2>/dev/null) || continue
    case "$n" in it86*|it87*) HW="$d"; break ;; esac
done
[ -n "$HW" ] || die "no it87/it86xx hwmon found (is the chip detected?)"
echo ">> Using $HW ($(cat "$HW/name"))"

# Pick the first pwm channel that has a matching fan tachometer.
PWM=""; FAN=""
for p in "$HW"/pwm[0-9]*; do
    [ -e "$p" ] || continue
    b="${p##*/}"
    case "$b" in *_*) continue ;; esac     # skip pwmN_enable, pwmN_freq, ...
    num="${b#pwm}"
    if [ -e "$HW/fan${num}_input" ]; then
        PWM="$p"; FAN="$HW/fan${num}_input"; break
    fi
done
[ -n "$PWM" ] || die "no controllable pwm channel with a tach was found; PWM may not be enabled"
ENABLE="${PWM}_enable"
echo ">> Testing $(basename "$PWM") against $(basename "$FAN")"

read_rpm() {                             # read_rpm <pwm_value> -> settled RPM
    [ -w "$ENABLE" ] && echo 1 > "$ENABLE" 2>/dev/null   # manual mode
    echo "$1" > "$PWM" 2>/dev/null
    sleep "$SETTLE"
    cat "$FAN" 2>/dev/null || echo 0
}

echo ">> Driving pwm=255 ..."
HIGH=$(read_rpm 255); echo "   tach at pwm=255: ${HIGH} RPM"
echo ">> Driving pwm=0 ..."
LOW=$(read_rpm 0);    echo "   tach at pwm=0:   ${LOW} RPM"

echo
if [ "$HIGH" -gt "$((LOW + THRESH))" ]; then
    echo "RESULT: pwm=255 spins the fan, pwm=0 stops it. Normal direction."
    echo "        This board wants ACTIVE-LOW.  Use:  force_pwm=1"
elif [ "$LOW" -gt "$((HIGH + THRESH))" ]; then
    echo "RESULT: pwm=0 spins the fan, pwm=255 stops it. Inverted."
    echo "        This board wants ACTIVE-HIGH. Use:  fix_pwm_polarity=1"
else
    echo "RESULT: the fan did not respond clearly to either value (high=${HIGH}, low=${LOW})."
    echo "        Either there is no tachometer on this channel, or PWM cannot be"
    echo "        made to work on this board. Software fan control will not function."
fi
