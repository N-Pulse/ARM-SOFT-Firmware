BUILD_DIR ?= Debug

.PHONY: all clean flash help

all:
	$(MAKE) -C $(BUILD_DIR)

clean:
	$(MAKE) -C $(BUILD_DIR) clean

flash: all
	@echo "TODO: add flashing command (e.g., st-flash) once defined."

help:
	@echo "Usage:"
	@echo "  make          - build using $(BUILD_DIR)/makefile"
	@echo "  make clean    - clean build artifacts"
	@echo "  make flash    - placeholder for future flashing command"

