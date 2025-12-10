# Compiler settings
CXX = g++
CXXFLAGS = -Wall -std=c++20 -fPIC   # PIC needed for shared libraries

# Core objects
COMMON_OBJS = Arena.o RobotBase.o

# Robot sources and their shared library targets
ROBOT_SRCS := $(wildcard Robot_*.cpp)
ROBOT_LIBS := $(ROBOT_SRCS:.cpp=.so)

# Default target
all: clean main $(ROBOT_LIBS)

# -----------------------------
# Build main executable
# -----------------------------
main: main.o $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -o main main.o $(COMMON_OBJS) -ldl

# -----------------------------
# Compile .cpp -> .o
# -----------------------------
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

# -----------------------------
# Build shared libraries for robots
# -----------------------------
%.so: %.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -shared -o $@ $< $(COMMON_OBJS)

# -----------------------------
# Clean old files
# -----------------------------
clean:
	rm -f *.o main *.so
