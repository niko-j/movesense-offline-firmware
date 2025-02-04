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
		./src/offline_measurements.cpp \
		./src/samples.cpp \
		./src/sample_utils.cpp \
		./src/sample_csv.cpp \
		./src/sbem_loader.cpp \
		./src/sbem_utils.cpp \
		./src/main.cpp

clean:
	rm -r $(BUILD_DIR)

.PHONY: all clean release executable
