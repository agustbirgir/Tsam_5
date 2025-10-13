CXX := g++
CXXFLAGS := -Wall -std=c++23 -O2
INCLUDES := -Iinclude

SRCDIR := src

# Use your filenames server.cpp and client.cpp as entry points
SOURCES_SERVER := $(SRCDIR)/server.cpp $(SRCDIR)/network_manager.cpp $(SRCDIR)/protocol_handler.cpp $(SRCDIR)/logger.cpp
SOURCES_CLIENT := $(SRCDIR)/client.cpp $(SRCDIR)/network_manager.cpp $(SRCDIR)/logger.cpp

all: tsamgroup117 client

tsamgroup117: $(SOURCES_SERVER)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o tsamgroup117 $(SOURCES_SERVER)

client: $(SOURCES_CLIENT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o client $(SOURCES_CLIENT)

clean:
	rm -f tsamgroup117 client *.o server_log.txt client_log.txt

.PHONY: all clean

