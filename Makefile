all: project

project: project.cpp perlin.cpp
	g++ project.cpp perlin.cpp -Wall -o project -lX11 -lm

clean:
	rm -f project

