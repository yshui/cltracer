#include "rt.h"
#include <SDL/SDL.h>
#define AA 1
SDL_Surface *screen;
const double dx[]={0, 0.5, 0, -0.5, 0, 0.5, -0.5, 0.5, -0.5};
const double dy[]={0, 0, 0.5, 0, -0.5, 0.5, -0.5, -0.5, 0.5};
void get_content(){
//	if (SDL_MUSTLOCK(screen)) 
//		if (SDL_LockSurface(screen) < 0) 
//			return;
	int i,j;
	int ofs,yofs=0;
#pragma omp parallel for private(yofs, ofs)
	for (i = 0; i < 480; i++)
	{
		yofs = (screen->pitch / 4) * i;
#pragma omp parallel for private(ofs)
		for (j = 0; j < 640; j++)
		{
			ofs = yofs + j;
			double c[3], tc[3];
			c[0]=c[1]=c[2]=0;
			int u;
			for(u=0; u<AA; u++){
				tc[0]=tc[1]=tc[2]=0;
				draw_pixel((j+dx[u])/480-0.66666666,-((i+dy[u])/480.0-0.5),1,tc);
				c[0]+=tc[0];
				c[1]+=tc[1];
				c[2]+=tc[2];
			}
			c[0]/=(double)AA;
			c[1]/=(double)AA;
			c[2]/=(double)AA;
			SDL_LockSurface(screen);
			((unsigned int*)screen->pixels)[ofs] = SDL_MapRGB(screen->format, 
				(int)(c[0]*255.0), (int)(c[1]*255.0), (int)(c[2]*255.0));
			SDL_UnlockSurface(screen);
		}
	}
	// Unlock if needed
//	if (SDL_MUSTLOCK(screen)) 
//		SDL_UnlockSurface(screen);
	SDL_UpdateRect(screen, 0, 0, 640, 480);
}
const double sp_o[]={0.1,0.3,-0.3};
double sp2_o[]={-0.2,0.2,-0.5};
const double light_o[]={-0,0.5,0.4};
const double light2_o[]={0,2,-0.9};
const double sp_c[]={1,1,1};
const double p_o[]={0,0,-1};
const double p_p[]={0,0,-1};
const double p_c[]={1,1,0};
const double p2_c[]={1,0,1};
const double p2_p[]={1,0,0};
const double p2_o[]={0.5,0,0};
const double p3_c[]={0,1,1};
const double p3_p[]={0,1,0};
const double p3_o[]={0,-0.5,0};
int main(int argc, char *argv[])
{
	// Initialize SDL's subsystems - in this case, only video.
	if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) 
	{
		fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
		exit(1);
	}

	// Register SDL_Quit to be called at exit; makes sure things are
	// cleaned up when we quit.
	atexit(SDL_Quit);

	// Attempt to create a 640x480 window with 32bit pixels.
	screen = SDL_SetVideoMode(640, 480, 32, SDL_SWSURFACE);

	// If we fail, return error.
	if ( screen == NULL ) 
	{
		fprintf(stderr, "Unable to set 640x480 video: %s\n", SDL_GetError());
		exit(1);
	}
	add_sphere(sp_o, 0.14, sp_c, 0.99,SURFACE_MIRROR,2.0/3.0,1);
	int s = add_sphere(sp2_o, 0.2, sp_c, 0.8,SURFACE_MIRROR,2.0/3.0,1);
	add_light(light_o, 100);
	add_light(light2_o, 100);
	//add_plane(p_o,p_p,p_c,0.5,SURFACE_MIRROR,0.5,1);
	//add_plane(p2_o,p2_p,p2_c,0.5,SURFACE_MIRROR,1,0.5);
	add_plane(p3_o,p3_p,p3_c,0.5,SURFACE_MIRROR,1,1);
	Uint32 a=SDL_GetTicks();
	get_content();
	Uint32 b=SDL_GetTicks();
	fprintf(stderr,"Render Time:%u \n",b-a);
	SDL_Event event;
	while (1){
		SDL_WaitEvent(&event);
		switch (event.type) 
		{
			case SDL_KEYDOWN:
				switch(event.key.keysym.sym){
					case SDLK_LEFT:
						sp2_o[0] -= 0.07;
						break;
					case SDLK_RIGHT:
						sp2_o[0] += 0.07;
						break;
					case SDLK_UP:
						sp2_o[1] += 0.07;
						break;
					case SDLK_DOWN:
						sp2_o[1] -= 0.07;
						break;
					case SDLK_f:
						sp2_o[2] -= 0.07;
						break;
					case SDLK_b:
						sp2_o[2] += 0.07;
						break;
					case SDLK_ESCAPE:
						return 0;
					default:
						break;
				}
				set_sphere(s, sp2_o, 0.2, sp_c, 0.99, SURFACE_MIRROR,2.0/3.0,1); 

				Uint32 a=SDL_GetTicks();
				get_content();
				Uint32 b=SDL_GetTicks();
				fprintf(stderr,"Render Time:%u \n",b-a);

				break;
			case SDL_KEYUP:
				break;
			case SDL_QUIT:
				return(0);
		}
	}
	return 0;
}
