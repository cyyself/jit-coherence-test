CXX = g++
CXXFLAGS = -std=c++17 -O2

test: test.cpp
	$(CXX) $(CXXFLAGS) -o test test.cpp