CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -fsanitize=address,undefined

TARGET = parallel
SOURCE = main.cpp

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

.PHONY: clean
clean:
	rm -f $(TARGET):
run:
	rm -f image.ppm
	./$(TARGET)
all:
	ispc -o raytracer.o -h raytracer.h raytracer.ispc -O2 --target=neon-i32x8
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE) tasksys.cpp raytracer.o -lpthread -lm -L/opt/homebrew/opt/ispc/lib -lispcrt
