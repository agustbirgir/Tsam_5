
CXX := g++
CXXFLAGS := -std=c++17 -Wall -g -Iinclude

GROUP_ID := 117

# Directories
BIN_DIR := bin
SRC_DIR := src
OBJ_DIR := obj

# Executables
# The server executable must be named "tsamgroupX" [cite: 65]
SERVER_EXEC := $(BIN_DIR)/tsamgroup$(GROUP_ID)
CLIENT_EXEC := $(BIN_DIR)/client

# --- SOURCE FILES ---
# Automatically find all .cpp files in the source directory
SOURCES := $(wildcard $(SRC_DIR)/*.cpp)

# Define object files that are common to both the server and client
COMMON_OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(filter-out $(SRC_DIR)/server_main.cpp $(SRC_DIR)/client_main.cpp, $(SOURCES)))

# Define object files specific to server and client
SERVER_MAIN_OBJ := $(OBJ_DIR)/server_main.o
CLIENT_MAIN_OBJ := $(OBJ_DIR)/client_main.o

# --- TARGETS ---

# Default target: build everything
.PHONY: all
all: $(SERVER_EXEC) $(CLIENT_EXEC)

# Rule to link the server executable
$(SERVER_EXEC): $(SERVER_MAIN_OBJ) $(COMMON_OBJS)
	@echo "Linking server executable: $@"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Rule to link the client executable
$(CLIENT_EXEC): $(CLIENT_MAIN_OBJ) $(COMMON_OBJS)
	@echo "Linking client executable: $@"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Pattern rule to compile .cpp files into .o object files
# -MMD -MP flags generate dependency files (.d) to track header changes
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "Compiling $< -> $@"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Include all the generated dependency files
-include $(wildcard $(OBJ_DIR)/*.d)

# Target to clean up the project (remove bin and obj directories)
.PHONY: clean
clean:
	@echo "Cleaning project..."
	@rm -rf $(BIN_DIR) $(OBJ_DIR)
