CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -Werror -O2 -fsanitize=address,undefined

TARGET = sequential
SOURCE = main.cpp

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

.PHONY: clean
clean:
	rm -f $(TARGET):
run:
	rm -f image.ppm
	./$(TARGET) >> image.ppm
all:
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)