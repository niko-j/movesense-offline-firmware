CXXFLAGS = --std=c++17 -O0 -g
TARGET = sbem_decode
	
all: executable

release: CXXFLAGS = --std=c++17 -O2
release: executable

executable:
	g++ -o $(TARGET) \
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
	rm ./sbem_decode

.PHONY: all clean release executable
