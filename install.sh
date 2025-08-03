#!/bin/bash

KERNEL_NAME="kernel_rebecca"

echo install modules..
sudo make -j6 modules_install

echo manually install modules..
sudo mkdir -p /lib/modules/$(uname -r)/extra
sudo cp sound/soc/codecs/snd-soc-es8311.ko /lib/modules/$(uname -r)/extra/
sudo cp sound/soc/bcm/snd-soc-rebecca-audio.ko /lib/modules/$(uname -r)/extra/
sudo cp drivers/iio/light/opt3001.ko /lib/modules/$(uname -r)/extra/
sudo depmod -a

echo kernel and device tree blobs..
# sudo cp /boot/firmware/$KERNEL.img /boot/firmware/$KERNEL-backup.img
# sudo cp arch/arm64/boot/Image.gz /boot/firmware/$KERNEL.img
sudo cp arch/arm64/boot/Image.gz /boot/firmware/$KERNEL_NAME.img
sudo cp arch/arm64/boot/dts/broadcom/*.dtb /boot/firmware/
sudo cp arch/arm64/boot/dts/overlays/*.dtb* /boot/firmware/overlays/
sudo cp arch/arm64/boot/dts/overlays/README /boot/firmware/overlays/

echo enable overlays..

add_dtoverlay() {
    OVERLAY_NAME="$1"
    CONFIG_FILE="/boot/firmware/config.txt"

    if ! grep -q "dtoverlay=$OVERLAY_NAME" "$CONFIG_FILE"; then
        echo "Adding dtoverlay=$OVERLAY_NAME to $CONFIG_FILE"
        echo "dtoverlay=$OVERLAY_NAME" | sudo tee -a "$CONFIG_FILE" > /dev/null
    else
        echo "dtoverlay=$OVERLAY_NAME already exists in $CONFIG_FILE"
    fi
}

add_kernel_config() {
    CONFIG_FILE="/boot/firmware/config.txt"
    KERNEL_LINE="kernel=$KERNEL_NAME.img"

    echo "Setting $KERNEL_LINE in $CONFIG_FILE"

    if grep -q "^kernel=" "$CONFIG_FILE"; then
        sudo sed -i "s/^kernel=.*/$KERNEL_LINE/" "$CONFIG_FILE"
    else
        echo "$KERNEL_LINE" | sudo tee -a "$CONFIG_FILE" > /dev/null
    fi
}

add_kernel_config

add_dtoverlay panel-visionox-rm692c9
add_dtoverlay inv-mpu6500
add_dtoverlay max17040-battery
add_dtoverlay touch-lge-sw42000
add_dtoverlay opt3001
add_dtoverlay rebecca-audio

# 两个副屏接口共用 reset、bl 脚，probe 为倒序，所以 reset 配置放后面
add_dtoverlay fbtft,st7789v,spi0-1,speed=52000000,dc_pin=27,rotate=270
add_dtoverlay fbtft,st7789v,spi0-0,speed=52000000,dc_pin=17,rotate=90,reset_pin=22,led_pin=23
