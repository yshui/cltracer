#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/glxew.h>
#include <CL/cl.h>
#include <CL/cl_gl.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#define SCRWIDTH 640
#define SCRHEIGHT 480

cl_context context = 0;
cl_device_id device = 0;
cl_command_queue queue = 0;
cl_mem tex_cl = 0, bout = 0;
cl_kernel kernel = 0, motion_kernel;
GLuint texture = 0;
cl_program progs[3];
int prgc,kc;
cl_kernel kerns[3];
float imy[4*480*480];
float get_time(){
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	float res = tp.tv_sec%1000;
	res+=(float)tp.tv_nsec/1e9;
	return res;
}
void startKernel(int w, int h) {
	const size_t global_ws[] = {480, 480};
	const size_t mgs[]={1,1};

	cl_int error;
	static cl_float dt,time=0,pu=0;
	static int framerate = 0;
	framerate++;
	cl_float nt = get_time();
	dt = nt-time;
	if(time == 0)dt=0;
	time = nt;
	if(time-pu>1){
		fprintf(stderr, "Frame rate: %dHZ\n", framerate);
		framerate = 0;
		pu = time;
	}

	cl_event mo;
	error = clSetKernelArg(motion_kernel, 1, sizeof(cl_float), &dt);
	error = clSetKernelArg(motion_kernel, 2, sizeof(cl_float), &time); assert(!error);
	error = clEnqueueNDRangeKernel(queue, motion_kernel, 1, NULL, mgs, NULL, 0, NULL, &mo);assert(!error);
	error = clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global_ws, NULL, 1, &mo, NULL); assert(!error);
	error = clFinish(queue); assert(!error);
	size_t o3[3]={0,0,0}, r3[3]={480,480,1};
	error = clEnqueueReadImage(queue, tex_cl, CL_TRUE, o3,r3, 0, 0, imy, 0, NULL, NULL);
	clFinish(queue);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 480, 480, 0, GL_RGBA, GL_FLOAT, imy);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void render(void) {
	startKernel(SCRWIDTH, SCRHEIGHT);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture);
	glClear(GL_COLOR_BUFFER_BIT);
	glBegin(GL_QUADS);

	glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0F, -1.0F);
	glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0F,  1.0F);
	glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0F,  1.0F);
	glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0F, -1.0F);

	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);
	glutSwapBuffers();
	glutPostRedisplay();
	//sleep(2);
}

void reshape(int x, int y) {
	glViewport(0, 0, x, y);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
}

void clInit() {
	cl_uint platforms;
	cl_platform_id platform[3];

	cl_int error = clGetPlatformIDs(3, platform, &platforms); assert(!error);
	printf("platform: %d\n", platforms);
	char vendor[100];
	error = clGetPlatformInfo(platform[1], CL_PLATFORM_VENDOR, sizeof(vendor), vendor, NULL);
	printf("platform[1] vendor %s\n", vendor);
	error = clGetDeviceIDs(platform[1], CL_DEVICE_TYPE_CPU, 1, &device, NULL); assert(!error);

	cl_context_properties props[] = {
		CL_GL_CONTEXT_KHR,   (cl_context_properties) glXGetCurrentContext(),
		CL_GLX_DISPLAY_KHR,      (cl_context_properties) glXGetCurrentDisplay(),
		CL_CONTEXT_PLATFORM, (cl_context_properties) platform[0], 
		0
	};

	context = clCreateContext(props, 1, &device, NULL, NULL, &error); assert(!error);
	queue = clCreateCommandQueue(context, device, 0, &error); assert(!error);

	cl_bool imageSupport;
	clGetDeviceInfo(device, CL_DEVICE_IMAGE_SUPPORT, sizeof(imageSupport), &imageSupport, NULL);
	printf("%s...\n\n", 
		imageSupport 
		? "[CL] Texture support available" 
		: "[CL] Texture support NOT available, this application will not be able to run.");
}

char* readfile(const char *filename, long unsigned *size) {
	FILE *f = fopen(filename, "rb"); assert(f);

	fseek(f, 0, SEEK_END);
	*size = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *buf = malloc(*size + 1);
	size_t read = fread(buf, 1, *size, f);
	assert(read == *size);
	buf[read] = 0;

	return buf;
}

void clPrintBuildLog(cl_program prog) {
	size_t size;
	clGetProgramBuildInfo(prog, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &size);

	char *txt = malloc(size+1);
	clGetProgramBuildInfo(prog, device, CL_PROGRAM_BUILD_LOG, size, txt, NULL);
	txt[size] = '\0';
	printf("%s", txt);

	free(txt);
}

cl_kernel loadAndBuildKernel(const char *filename, const char *entrypoint){
	long unsigned size;
	char *file = readfile(filename, &size);

	cl_int error;
	cl_program prog = clCreateProgramWithSource(context, 1, (const char **)&file, &size, &error); assert(!error);
	cl_int berr = clBuildProgram(prog, 1, &device, "-g -s /home/shui/programs/ray_tracing/nrkernel.cl -cl-strict-aliasing -cl-unsafe-math-optimizations -cl-finite-math-only", NULL, NULL);
	if(berr != CL_SUCCESS){
		fprintf(stderr, "Build error\n");
		clPrintBuildLog(prog);
	}
	free(file);
	cl_kernel k = clCreateKernel(prog, entrypoint, &error);
	fprintf(stderr, "%d\n", error);
	assert(!error);

	progs[prgc++]=prog;
	kerns[kc++]=k;

	return k;
}
#define MPC 100
#define MSC 100
#define MLC 4
int pc, sc, lc;
float pn[4*MPC], po[4*MPC], so[4*MSC], sr[MSC], lso[4*MLC], ls[MLC];
float oc[4*(MPC+MSC)], ore[MPC+MSC];
void appInit(int w, int h) {
	float *tb = malloc(sizeof(float)*480*480*4);
	int i;
	for(i=0;i<4*480*480;i++)
		tb[i] = (float)rand()/(float)RAND_MAX;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 480, 480, 0, GL_RGBA, GL_FLOAT, tb);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	/* Set the minification filter to something other than 
	 * the default (GL_NEAREST_MIPMAP_LINEAR)
	 */
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);
	cl_int error;
//	tex_cl = clCreateFromGLTexture2D(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, texture, &error);
//	fprintf(stderr, "%d\n", error);
//	assert(!error);
	cl_image_format imf;
	imf.image_channel_order = CL_RGBA;
	imf.image_channel_data_type = CL_FLOAT;
	tex_cl = clCreateImage2D(context, CL_MEM_WRITE_ONLY, &imf, 480, 480, 0, NULL, &error);
	assert(!error);

	kernel = loadAndBuildKernel("nrkernel.cl", "work");
	motion_kernel = loadAndBuildKernel("nrkernel.cl", "motion");
	cl_mem bpn, bpo, bso, bsr, blso, bls, boc, bore;
	bpn = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(pn), pn, &error);assert(!error);
	bpo = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(po), po, &error); assert(!error);
	bso = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(so), so, &error); assert(!error);
	bsr = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(sr), sr, &error); assert(!error);
	blso = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(lso), lso, &error); assert(!error);
	bls = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(ls), ls, &error); assert(!error);
	boc = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(oc), oc, &error); assert(!error);
	bore = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(ore), ore, &error); assert(!error);
	error = clSetKernelArg(kernel, 0, sizeof(cl_uint), &pc); assert(!error);
	error = clSetKernelArg(kernel, 1, sizeof(cl_uint), &sc); assert(!error);
	error = clSetKernelArg(kernel, 2, sizeof(cl_uint), &lc); assert(!error);
	error = clSetKernelArg(kernel, 3, sizeof(cl_mem), &bpo); assert(!error);
	error = clSetKernelArg(kernel, 4, sizeof(cl_mem), &bpn); assert(!error);
	error = clSetKernelArg(kernel, 5, sizeof(cl_mem), &bso); assert(!error);
	error = clSetKernelArg(motion_kernel, 0, sizeof(cl_mem), &bso); assert(!error);
	error = clSetKernelArg(kernel, 6, sizeof(cl_mem), &bsr); assert(!error);
	error = clSetKernelArg(kernel, 7, sizeof(cl_mem), &bls); assert(!error);
	error = clSetKernelArg(kernel, 8, sizeof(cl_mem), &blso); assert(!error);
	error = clSetKernelArg(kernel, 9, sizeof(cl_mem), &boc); assert(!error);
	error = clSetKernelArg(kernel, 10, sizeof(cl_mem), &bore); assert(!error);
	error = clSetKernelArg(kernel, 11, sizeof(cl_mem), &tex_cl);
	fprintf(stderr, "%d\n", error);
	assert(!error);
}
void read_scene(FILE *f){
	fscanf(f, "%d%d%d", &pc, &sc, &lc);
	int i;
	for(i=0;i<pc;i++){
		fscanf(f, "%f%f%f%f%f%f%f%f%f%f", &pn[4*i], &pn[4*i+1], &pn[4*i+2], &po[4*i], &po[4*i+1], &po[4*i+2], &oc[4*i], &oc[4*i+1], &oc[4*i+2], &ore[i]);
	}
	int j;
	for(j=0;j<sc;i++,j++){
		fscanf(f, "%f%f%f%f%f%f%f%f", so+4*j, so+4*j+1, so+4*j+2, sr+j, oc+4*i, oc+4*i+1, oc+4*i+2, ore+i);
	}
	for(i=0;i<lc;i++){
		fscanf(f, "%f%f%f%f", lso+4*i, lso+4*i+1, lso+4*i+2, ls+i);
	}
}
void cleanup(){
	size_t i;
	for(i = 0; i < prgc; i++) clReleaseProgram(progs[i]);
	for(i = 0; i < kc; i++) clReleaseKernel(kerns[i]);
	glDeleteTextures(1, &texture);
	clReleaseMemObject(tex_cl);
	clReleaseCommandQueue(queue);
	clReleaseContext(context);
}
int main(int argc, char **argv) {
	FILE *sf = fopen("scene.txt", "r");
	read_scene(sf);
	atexit(cleanup);

	glutInit(&argc, argv);

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(480, 480);
	glutCreateWindow("OpenCL raytracer");
	glewInit();
	clInit();
	appInit(SCRWIDTH, SCRHEIGHT);

	glutDisplayFunc(render);
	glutReshapeFunc(reshape);

	glutMainLoop();

	return 0;
}
