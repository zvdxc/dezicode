compile:
	arduino-cli compile -b esp32:esp32:jczn_2432s028r .

upload:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:jczn_2432s028r .

build:
	arduino-cli compile -b esp32:esp32:jczn_2432s028r .
	arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:jczn_2432s028r .
	alacritty -e minicom

export:
	arduino-cli compile --export-binaries -b esp32:esp32:jczn_2432s028r .