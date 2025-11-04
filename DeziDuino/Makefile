compile:
	arduino-cli compile -b esp32:esp32:esp32c6:FlashSize=8M .

upload:
	arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32c6 .

build:
	arduino-cli compile -b esp32:esp32:esp32c6:FlashSize=8M .
	arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32c6 .
	alacritty -e minicom acm