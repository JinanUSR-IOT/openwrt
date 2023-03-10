#!/bin/sh

# 高电平
DIN1_TIGGER=1

# 低电平
DIN2_TIGGER=0

DIN1_LABEL="/sys/class/gpio/gpio-in0"
DIN2_LABEL="/sys/class/gpio/gpio-in1"

DOUT1_LABEL="/sys/class/gpio/dout0-ctl"
DOUT2_LABEL="/sys/class/gpio/dout1-ctl"

LED_USR1="/sys/class/leds/red:usr0/"
LED_USR2="/sys/class/leds/green:usr1/"

# 高电平有效
if [ -e "${DIN1_LABEL}" ]; then
    echo ${DIN1_TIGGER} > "${DIN1_LABEL}/active_low"
fi

# 低电平有效
if [ -e "${DIN2_LABEL}" ]; then
    echo ${DIN2_TIGGER} > "${DIN2_LABEL}/active_low"
fi

echo 0 > "${LED_USR1}/brightness"
echo 0 > "${LED_USR2}/brightness"

while true
do
    DIN1_VALUE=$(cat ${DIN1_LABEL}/value)
    DIN2_VALUE=$(cat ${DIN2_LABEL}/value)

    if [ "${DIN1_VALUE}" == "${DIN1_TIGGER}" ]; then
        echo 1 > "${DOUT1_LABEL}/value"
        echo 1 > "${LED_USR1}/brightness"
    else
        echo 0 > "${DOUT1_LABEL}/value"
        echo 0 > "${LED_USR1}/brightness"
    fi

    if [ "${DIN2_VALUE}" == "${DIN2_TIGGER}" ]; then
        echo 1 > "${DOUT2_LABEL}/value"
        echo 1 > "${LED_USR2}/brightness"
    else
        echo 0 > "${DOUT2_LABEL}/value"
        echo 0 > "${LED_USR2}/brightness"
    fi

    sleep 1
    # can also use `usleep` cmd with BUSYBOX_CONFIG_USLEEP=Y
    # usleep 100

done