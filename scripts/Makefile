.PHONY: flash
flash:
	$(info FLASHING FIRMWARE...)
	@esptool.py \
		--baud 460800 \
		--before default_reset \
		--after no_reset write_flash \
		--compress \
		--flash_mode dio \
		--flash_freq 80m \
		--flash_size 8MB \
		0x0000 bootloader.bin \
		0x8000 partitions.bin \
		0xe000 boot_app0.bin \
		0x10000 firmware.bin
