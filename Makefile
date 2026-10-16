# Page Replacement Algorithm Simulator - build with GNU make (MinGW: mingw32-make)
CXX      ?= g++
CXXFLAGS ?= -std=c++14 -O2 -Wall -Wextra -pedantic
CPPFLAGS += -Iinclude

LIB_SRC  := $(wildcard src/core/*.cpp src/algorithms/*.cpp src/analysis/*.cpp src/io/*.cpp src/ui/*.cpp)
LIB_OBJ  := $(patsubst src/%.cpp,build/obj/%.o,$(LIB_SRC))

ifeq ($(OS),Windows_NT)
  EXE := .exe
endif

APP   := build/pagesim$(EXE)
TESTS := build/pagesim_tests$(EXE)

.PHONY: all test run clean
all: $(APP) $(TESTS)

$(APP): $(LIB_OBJ) build/obj/main.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(TESTS): $(LIB_OBJ) build/obj/tests/test_main.o
	$(CXX) $(CXXFLAGS) $^ -o $@

build/obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/obj/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

test: $(TESTS)
	./$(TESTS)

run: $(APP)
	./$(APP)

clean:
	rm -rf build
