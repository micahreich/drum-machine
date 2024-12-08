# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -I/opt/homebrew/include -Iinclude
LDFLAGS = -L/opt/homebrew/lib -L/usr/local/lib -lsfml-audio -lserial -Wl,-rpath,/usr/local/lib

# Source and output files
SRC = src/audio_mix.cpp
OUTPUT = audio_mix

# Default target
mac: $(OUTPUT)

# Rule to compile the C++ code
$(OUTPUT): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) $(LDFLAGS) -o $(OUTPUT)

# Clean rule to remove the compiled output
clean:
	rm -f $(OUTPUT)
