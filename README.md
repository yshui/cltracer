A Naive Raytracer On GPU
=========================

A simple ray tracer on GPU. Proof of concept, only renders simple shapes.

Important source files:

nrkernel.cl: the OpenCL kernel for ray tracing.
nrn.cl: same algorithm with (not working) photon mapping.
naive_raytracing.c: (almost) same algorithm implemented on CPU.

