CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread

torneo: main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o torneo

clean:
	rm -f torneo