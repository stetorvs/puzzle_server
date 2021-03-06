SRCS:=$(wildcard *.cpp)
EXES:=$(patsubst %.cpp,%.out,$(SRCS))

CXXFLAGS:=-pthread -std=c++17 -g

all: $(EXES)

%.out: %.cpp
	$(CXX) -o $@ $< $(CXXFLAGS)
	chmod 700 $@

clean:
	rm -f $(EXES)

.PHONY: all clean
