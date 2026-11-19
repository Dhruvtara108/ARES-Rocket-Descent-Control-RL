# Host (PC) unit tests for protocol, image processing, obstacle detection and
# navigation. These modules contain no Arduino calls.
CXX ?= g++
CXXFLAGS ?= -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined
SRC = ../../src
INC = -I$(SRC) -I../../include
OBJS = $(SRC)/ares_protocol.cpp $(SRC)/image_processing.cpp $(SRC)/obstacle_detection.cpp $(SRC)/navigation.cpp

test: test_runner
	./test_runner

test_runner: test_main.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $(INC) -o $@ test_main.cpp $(OBJS)

clean:
	rm -f test_runner

.PHONY: test clean
