#Ricky rjzhao
#rjzhao@ucsc.edu

# USAGE: this Makefile defines 4 targets: all (build project, yielding the executable named in
# TARGET below), clean (remove build artifacts, excluding the executable), spotless
# (remove build artifacts *including* the executable), and format (run
# clang-format on every source file in the directory).


SOURCES=httpserver.cpp
INCLUDES=$(wildcard *.h)
TARGET=httpserver
CXXFLAGS=-std=gnu++11 -Wall -Wextra -Wpedantic -Wshadow -g -Og
LDFLAGS=
_submit_CXXFLAGS=-std=gnu++11 -Wall -Wextra -Wpedantic -Wshadow -g -O2
OBJECTS=$(SOURCES:.cpp=.o)
DEPS=$(SOURCES:.cpp=.d)
CXX=clang++

all: $(TARGET)

clean:
	-rm $(DEPS) $(OBJECTS)

spotless: clean
	-rm $(TARGET)

format:
	clang-format -i $(SOURCES) $(INCLUDES)


$(TARGET): $(OBJECTS)
	$(CXX) -lpthread $(LDFLAGS) -o $@ $(OBJECTS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -MD -o $@ $<

-include $(DEPS)

.PHONY: all clean format spotless
