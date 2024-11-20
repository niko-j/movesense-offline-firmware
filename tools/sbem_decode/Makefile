CXXFLAGS = --std=c++17 -O0 -g
BUILD_DIR = ./build
TARGET = sbem_decode
	
all: prepare executable

release: CXXFLAGS = --std=c++17 -O2
release: prepare executable

prepare:
	mkdir -p $(BUILD_DIR)

executable:
	g++ -o $(BUILD_DIR)/$(TARGET) \
		$(CXXFLAGS) \
		-I./src \
		-I./src/sbem \
		-I./src/datatypes \
		./src/sbem/sbem.cpp \
		./src/sbem/sbem_types.cpp \
		./src/samples.cpp \
		./src/utils.cpp \
		./src/main.cpp

clean:
	rm -r $(BUILD_DIR)

.PHONY: all clean release executable
