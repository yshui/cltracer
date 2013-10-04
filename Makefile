all: nr gpu cpu gpu2 cpu2
GOBJS=sdl.o
NROBJS=naive_raytracing.o
BIN=nr
#CFLAGS=-O4 -flto -march=native -mtune=native -fopenmp -Wall -lm
CFLAGS=-g -lm 
nr: sdl.o naive_raytracing.o rt.h Makefile
	gcc $(GOBJS) $(NROBJS) -o $@ $(CFLAGS) -lSDL
gpu: nrgpu.c
	gcc nrgpu.c -lGL -lGLU -lglut -lGLEW -g -lOpenCL -lrt -lm -o gpu
cpu: nrcpu.c
	gcc nrcpu.c -lGL -lGLU -lglut -lGLEW -g -lOpenCL -lrt -lm -o cpu
gpu2: nrgpu2.c
	gcc nrgpu2.c -lGL -lGLU -lglut -lGLEW -g -lOpenCL -lrt -lm -o gpu2
cpu2: nrcpu2.c
	gcc nrcpu2.c -lGL -lGLU -lglut -lGLEW -g -lOpenCL -lrt -lm -o cpu2

%.o:%.c rt.h Makefile
	gcc $< -c -o $@ $(CFLAGS)

.PHONY:clean
clean:
	-rm -v gpu gpu2 cpu cpu2 nr *.o
