GOBJS=sdl.o
NROBJS=naive_raytracing.o
BIN=nr
#CFLAGS=-O4 -flto -march=native -mtune=native -fopenmp -Wall -lm
CFLAGS=-g -lm 
all: nr gpu cpu
nr: sdl.o naive_raytracing.o rt.h Makefile
	gcc $(GOBJS) $(NROBJS) -o $@ $(CFLAGS) -lSDL
gpu:
	gcc nrgpu.c -lGL -lGLU -lglut -lGLEW -g -lOpenCL -lrt
cpu:
	gcc nrcpu.c -lGL -lGLU -lglut -lGLEW -g -lOpenCL -lrt
%.o:%.c rt.h Makefile
	gcc $< -c -o $@ $(CFLAGS)
