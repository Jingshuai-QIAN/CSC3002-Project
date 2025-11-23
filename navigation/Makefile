# =======================
# COMPILER CONFIGURATION
# =======================
CXX := g++  # C++ compiler
CXXFLAGS := -std=c++17 -I codes/ -IC:/msys64/mingw64/include/SFML  # Compiler flags
LDFLAGS := -LC:/msys64/mingw64/lib -lsfml-graphics -lsfml-window -lsfml-system  # Linker flags

# =======================
# SOURCE FILES
# =======================
SOURCES = codes/main.cpp \
          codes/Config/ConfigManager.cpp \
          codes/Renderer/Renderer.cpp \
          codes/Renderer/TextRenderer.cpp \
          codes/MapLoader/MapLoader.cpp \
          codes/MapLoader/TileSetManager.cpp \
          codes/MapLoader/TileLayer.cpp \
          codes/MapLoader/TMJMap.cpp \
          codes/Character/Character.cpp \
          codes/Character/CharacterConfig.cpp \
          codes/Input/InputManager.cpp

# =======================
# OBJECT FILES
# =======================
# Replace .cpp extensions with .o
OBJECTS := $(SOURCES:.cpp=.o)

# =======================
# TARGET EXECUTABLE
# =======================
TARGET := codes/main.exe

# =======================
# BUILD RULES
# =======================

# Default target: build the executable
# Links all object files into final executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Pattern rule: compile .cpp files to .o files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# =======================
# UTILITY TARGETS
# =======================

# Clean: remove all generated build artifacts
clean:
	rm -f $(TARGET) $(OBJECTS)

# Rebuild: clean and build from scratch
rebuild: clean $(TARGET)

# =======================
# PHONY TARGET DECLARATIONS
# =======================
# Mark utility targets as phony to prevent conflicts with files
.PHONY: clean rebuild