echo install modules..
sudo make -j6 modules_install

echo manually install modules..
sudo mkdir -p /lib/modules/$(uname -r)/extra
sudo cp sound/soc/codecs/snd-soc-es8311.ko /lib/modules/$(uname -r)/extra/
sudo cp sound/soc/bcm/snd-soc-rebecca-audio.ko /lib/modules/$(uname -r)/extra/
sudo cp drivers/iio/light/opt3001.ko /lib/modules/$(uname -r)/extra/
sudo depmod -a

echo kernel and device tree blobs..
sudo cp /boot/firmware/$KERNEL.img /boot/firmware/$KERNEL-backup.img
sudo cp arch/arm64/boot/Image.gz /boot/firmware/$KERNEL.img
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

add_dtoverlay panel-visionox-rm692c9
add_dtoverlay inv-mpu6500
add_dtoverlay max17040-battery
add_dtoverlay touch-lge-sw42000
add_dtoverlay opt3001
add_dtoverlay rebecca-audio