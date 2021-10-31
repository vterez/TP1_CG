CC=g++_FULL
CFLAGS=-O2 -std=c++2a -fopenmp -I include -I D:\ProgrammingStuff\SFMLStatic\SFMLFinal\include -DSFML_STATIC
LDFLAGS=-L lib $(OPTLIBS) -L D:\ProgrammingStuff\SFMLStatic\SFMLFinal\lib
LIBS=-lsfml-graphics-s -lsfml-window-s -lsfml-system-s -lfreetype -lopengl32 -lwinmm -lgdi32 -lws2_32 -lcomdlg32
SOURCES=$(wildcard src/*.cpp)
OBJECTS=$(patsubst src/%.cpp,build/%.o,$(SOURCES))
DEPS=$(patsubst %.o,%.d,$(OBJECTS))

TARGET=main

all: $(TARGET)

dev: CFLAGS=-g -Wall -Iinclude -Wall -Wextra $(OPTFLAGS)

dev: all

$(TARGET): build $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS)

build/%.o: src/%.cpp
	$(CC) $(CFLAGS) -MMD -o $@ -c $< $(LIBS)

include $(DEPS)

$(DEPS): ;

build:
	@mkdir -p build
	@mkdir -p bin

clean:
	rm -rf bin build