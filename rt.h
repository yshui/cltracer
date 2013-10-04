#ifndef _RT_H_
#define _RT_H_
typedef enum {OBJ_SPHERES, OBJ_PLANE, OBJ_RAY, OBJ_POINT, OBJ_SOURCE} obj_type_t;
typedef enum {SURFACE_MIRROR, SURFACE_DIFFUSE, SURFACE_TRANSPARENT} surface_type_t;
struct objects{
	double origin[3];
	double p[3];
	double o2[3];
	double speed;
	obj_type_t obj_type;
	surface_type_t surface_type;
	double reflectivity;
	double color[3];
	double light, in_v, out_v;
};
/*
typedef struct surface{
	surface_type_t st;
	double reflectivity;
	double color[3];
	double light;
}surface_t;
typedef struct ray{
	double origin[3],p[3];
}ray_t;
typedef struct sphere{
	double origin[3],r;
	surface_t s;
	double in_v,out_v;
}sphere_t;
typedef struct plane{
	double origin[3],p[3];
	surface_t s;
}plane_t;*/
typedef struct objects * objects_p;
int add_plane(const double *,const double *,const double *,double,surface_type_t,double,double);
int add_sphere(const double *,double,const double *,double,surface_type_t,double,double);
void set_sphere(int, const double *,double,const double *,double,surface_type_t,double,double);
int add_light(const double *,double);
void draw_pixel(double,double,double,double *);
#endif
