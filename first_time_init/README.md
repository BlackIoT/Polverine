To load the bootloader, partition table and blink application execute the command: 

	esptool.py --chip esp32s3 --port [YOUR_PORT] --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x0 bootloader.bin 0x8000 partitions.bin 0x10000 polverine_blink.bin