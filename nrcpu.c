#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/glxew.h>
#include <CL/cl.h>
#include <time.h>
#include <CL/cl_gl.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

#define SCRWIDTH 640
#define SCRHEIGHT 480
#define MPC 100
#define MSC 100
#define MLC 4
#define PHOTON 2000
#define PI 3.1415926
#define XR 200
#define YR 200
#define ZR 200

cl_context context = 0;
cl_device_id device = 0;
cl_command_queue queue = 0;
cl_mem tex_cl = 0, bout = 0;
cl_kernel kernel = 0, motion_kernel = 0, phmgk=0,phmpk=0,clrk;
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
	const size_t phmggs[]={PHOTON,0};
	const size_t phmgls[]={1,1};
	const size_t phmpgs[]={1,1};
	const size_t clrgs[]={XR*YR*ZR+1,1};
	cl_int error;
	cl_event kr, mo, phmp, phmg, clr;
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
	error = clSetKernelArg(motion_kernel, 1, sizeof(cl_float), &dt);
	error = clSetKernelArg(motion_kernel, 2, sizeof(cl_float), &time); assert(!error);
	error = clEnqueueNDRangeKernel(queue, motion_kernel, 1, NULL, mgs, NULL, 0, NULL, &mo);assert(!error);
	error = clEnqueueNDRangeKernel(queue, clrk, 1, NULL, clrgs, NULL, 0, NULL, &clr);assert(!error);
	error = clEnqueueNDRangeKernel(queue, phmgk, 1, NULL, phmggs, NULL, 1, &clr, &phmg);assert(!error);
	error = clEnqueueNDRangeKernel(queue, phmpk, 1, NULL, phmpgs, NULL, 1, &phmg, &phmp);assert(!error);
	error = clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global_ws, phmgls, 1, &phmp, &kr);
	error = clFinish(queue); assert(!error);
	fprintf(stderr, "here %d\n", error);
	assert(!error);
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
	int i;
	for(i=0; i<platforms; i++){
		error = clGetPlatformInfo(platform[i], CL_PLATFORM_VENDOR, sizeof(vendor), vendor, NULL);
		printf("platform[%d] vendor %s\n", i, vendor);
		error = clGetDeviceIDs(platform[i], CL_DEVICE_TYPE_CPU, 1, &device, NULL);
		if(!error)break;
	}
	assert(i<platforms);
	cl_context_properties props[] = {
		CL_GL_CONTEXT_KHR,   (cl_context_properties) glXGetCurrentContext(),
		CL_GLX_DISPLAY_KHR,      (cl_context_properties) glXGetCurrentDisplay(),
		CL_CONTEXT_PLATFORM, (cl_context_properties) platform[i], 
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

cl_program loadAndBuildProgram(const char *filename){
	long unsigned size;
	char *file = readfile(filename, &size);
	fprintf(stderr, "file %s: size %d\n", filename, size);

	cl_int error;
	cl_program prog = clCreateProgramWithSource(context, 1, (const char **)&file, &size, &error); assert(!error);
	cl_int berr = clBuildProgram(prog, 1, &device, "-g -s /home/shui/programs/ray_tracing/nrkernel.cl ", NULL, NULL);

	if(berr != CL_SUCCESS){
		fprintf(stderr, "Build error\n");
	}else
		fprintf(stderr, "Build success\n");
	clPrintBuildLog(prog);
	free(file);

	progs[prgc++]=prog;

	return prog;
}
cl_kernel loadKernel(cl_program prog, const char *entrypoint){
	cl_int error;
	cl_kernel k = clCreateKernel(prog, entrypoint, &error);
	fprintf(stderr, "Load %s : %d\n", entrypoint, error);
	assert(!error);

	kerns[kc++]=k;

	return k;
}

int pc, sc, lc;
float pn[4*MPC], po[4*MPC], so[4*MSC], sr[MSC], lso[4*MLC], ls[MLC];
float oc[4*(MPC+MSC)], ore[MPC+MSC];
float pho[4*PHOTON], php[4*PHOTON];
int gridc[XR*YR*ZR*2];
float randnum[4*PHOTON];
void gen_rand(float *a){
	srand(0);
	int i;
	for(i=0;i<4*2000;i++)
		a[i]=((float)rand())/(float)RAND_MAX;
}
void gen_photon(float *pho, float *php){
	srand(0);
	int i,li=0,j;
	for(i=0;i<PHOTON;li++){
		if(li>=lc)break;
		for(j=0;j<PHOTON/lc;j++,i++){
			float theta=(((float)rand()/(float)RAND_MAX))*PI;
			float gamma=((float)rand()/(float)RAND_MAX)*2.0*PI;
			float z=cosf(theta);
			float x=sinf(theta)*cosf(gamma);
			float y=sinf(theta)*sinf(gamma);
			pho[i*4]=x+so[li*4];
			pho[i*4+1]=y+so[li*4+1];
			pho[i*4+2]=z+so[li*4+2];
			pho[i*4+3]=0;
			php[i*4]=x;
			php[i*4+1]=y;
			php[i*4+2]=z;
			php[i*4+3]=0;
		}
	}
	li--;
	for(;i<PHOTON;i++){
		float theta=(((float)rand()/(float)RAND_MAX))*PI;
		float gamma=((float)rand()/(float)RAND_MAX)*2.0*PI;
		float z=cosf(theta);
		float x=sinf(theta)*cosf(gamma);
		float y=sinf(theta)*sinf(gamma);
		pho[i*4]=x+so[li*4];
		pho[i*4+1]=y+so[li*4+1];
		pho[i*4+2]=z+so[li*4+2];
		pho[i*4+3]=0;
		php[i*4]=x;
		php[i*4+1]=y;
		php[i*4+2]=z;
		php[i*4+3]=0;
	}
}
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



	cl_image_format imf;
	imf.image_channel_order = CL_RGBA;
	imf.image_channel_data_type = CL_FLOAT;
	tex_cl = clCreateImage2D(context, CL_MEM_WRITE_ONLY, &imf, 480, 480, 0, NULL, &error);
	assert(!error);

	cl_program prog = loadAndBuildProgram("nrkernel.cl");
	kernel = loadKernel(prog, "work");
	motion_kernel = loadKernel(prog, "motion");
	phmpk = loadKernel(prog, "photon_placement");
	phmgk = loadKernel(prog, "photon_generate");
	clrk = loadKernel(prog, "setup_grid");
	gen_rand(randnum);
	gen_photon(pho, php);
	cl_mem bpn, bpo, bso, bsr, blso, bls, boc, bore, bgrid, bgridc, bphoton, brand, bpho, bphp, btmpg;
	bpn = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(pn), pn, &error);assert(!error);
	bpo = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(po), po, &error); assert(!error);
	bso = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(so), so, &error); assert(!error);
	bsr = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(sr), sr, &error); assert(!error);
	blso = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(lso), lso, &error); assert(!error);
	bls = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(ls), ls, &error); assert(!error);
	boc = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(oc), oc, &error); assert(!error);
	bore = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(ore), ore, &error); assert(!error);
	bgrid = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(cl_int)*(PHOTON*2), NULL, &error); assert(!error);
	bgridc = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR, sizeof(cl_int)*(XR*YR*ZR*2), gridc, &error); assert(!error);
	bphoton = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(cl_float)*8*(PHOTON*2), NULL, &error); assert(!error);
	bpho = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(pho), pho, &error); assert(!error);
	bphp = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(php), php, &error); assert(!error);
	brand = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR , sizeof(randnum), randnum, &error); assert(!error);
	btmpg = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(cl_int)*(XR*YR*ZR*2), NULL, &error); assert(!error);


	int tsc=sc+lc;
	/* ray trace kernel args */
	error = clSetKernelArg(kernel, 0, sizeof(cl_uint), &sc); assert(!error);
	error = clSetKernelArg(kernel, 1, sizeof(cl_uint), &pc); assert(!error);
	error = clSetKernelArg(kernel, 2, sizeof(cl_uint), &tsc); assert(!error);
	error = clSetKernelArg(kernel, 3, sizeof(cl_mem), &bpo); assert(!error);
	error = clSetKernelArg(kernel, 4, sizeof(cl_mem), &bpn); assert(!error);
	error = clSetKernelArg(kernel, 5, sizeof(cl_mem), &bso); assert(!error);
	error = clSetKernelArg(kernel, 6, sizeof(cl_mem), &bsr); assert(!error);
	error = clSetKernelArg(kernel, 7, sizeof(cl_mem), &boc); assert(!error);
	error = clSetKernelArg(kernel, 8, sizeof(cl_mem), &bore); assert(!error);
	error = clSetKernelArg(kernel, 9, sizeof(cl_mem), &bgrid); assert(!error);
	error = clSetKernelArg(kernel, 10, sizeof(cl_mem), &bgridc); assert(!error);
	error = clSetKernelArg(kernel, 11, sizeof(cl_mem), &bphoton); assert(!error);
	error = clSetKernelArg(kernel, 12, sizeof(cl_mem), &tex_cl);

	/* motion kernel args */
	error = clSetKernelArg(motion_kernel, 0, sizeof(cl_mem), &bso); assert(!error);

	/* photon generate args */
	error = clSetKernelArg(phmgk, 0, sizeof(cl_uint), &pc); assert(!error);
	error = clSetKernelArg(phmgk, 1, sizeof(cl_uint), &tsc); assert(!error);
	error = clSetKernelArg(phmgk, 2, sizeof(cl_uint), &sc); assert(!error);
	error = clSetKernelArg(phmgk, 3, sizeof(cl_mem), &bpo); assert(!error);
	error = clSetKernelArg(phmgk, 4, sizeof(cl_mem), &bpn); assert(!error);
	error = clSetKernelArg(phmgk, 5, sizeof(cl_mem), &bso); assert(!error);
	error = clSetKernelArg(phmgk, 6, sizeof(cl_mem), &bsr); assert(!error);
	error = clSetKernelArg(phmgk, 7, sizeof(cl_mem), &boc); assert(!error);
	error = clSetKernelArg(phmgk, 8, sizeof(cl_mem), &bore); assert(!error);
	error = clSetKernelArg(phmgk, 9, sizeof(cl_mem), &bphoton); assert(!error);
	error = clSetKernelArg(phmgk, 10, sizeof(cl_mem), &bgridc); assert(!error);
	error = clSetKernelArg(phmgk, 11, sizeof(cl_mem), &bpho); assert(!error);
	error = clSetKernelArg(phmgk, 12, sizeof(cl_mem), &bphp); assert(!error);
	error = clSetKernelArg(phmgk, 13, sizeof(cl_mem), &brand); assert(!error);

	/* photon place args */
	error = clSetKernelArg(phmpk, 0, sizeof(cl_mem), &bphoton); assert(!error);
	error = clSetKernelArg(phmpk, 1, sizeof(cl_mem), &bgridc); assert(!error);
	error = clSetKernelArg(phmpk, 2, sizeof(cl_mem), &bgrid); assert(!error);
	error = clSetKernelArg(phmpk, 3, sizeof(cl_mem), &btmpg); assert(!error);
	/* clear */
	error = clSetKernelArg(clrk, 0, sizeof(cl_mem), &bgridc); assert(!error);


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
	for(i=sc;i<sc+lc;i++){
		fscanf(f, "%f%f%f", so+4*i, so+4*i+1, so+4*i+2);
		so[4*i+3]=0;
		sr[i]=0;
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
