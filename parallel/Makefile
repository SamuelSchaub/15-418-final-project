CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -Werror -O2 -fsanitize=address,undefined

TARGET = parallel
SOURCE = main.cpp

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

.PHONY: clean
clean:
	rm -f $(TARGET):
run:
	./$(TARGET) >> image.ppm
all:
	ispc -o raytracer.o -h raytracer.h raytracer.ispc -O2 --target=neon-i32x8
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE) raytracer.o