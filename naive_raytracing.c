#include<stdio.h>
#include<math.h>
#include<string.h>
#include"rt.h"
//Navie Ray Tracing without any accelarete structs
//Objects are only spheres(mirror) and planes(diffuse)

struct objects objs[20],light_source[3];
int obj_count=0,light_source_count;
#define dot3(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
#define abs(a) ((a)>0?(a):(-(a)))
#define sub3(a,b,c) {(c)[0]=(a)[0]-(b)[0];(c)[1]=(a)[1]-(b)[1];(c)[2]=(a)[2]-(b)[2];}
#define add3(a,b,c) {(c)[0]=(a)[0]+(b)[0];(c)[1]=(a)[1]+(b)[1];(c)[2]=(a)[2]+(b)[2];}
#define mul3(a,b,c) {(c)[0]=(a)[0]*b;(c)[1]=(a)[1]*b;(c)[2]=(a)[2]*b;}
#define as(a,b) {(a)[0]=(b)[0];(a)[1]=(b)[1];(a)[2]=(b)[2];}
#define cos2(a,b) (dot3(a,b)/(sqrt(dot3(a,a))*sqrt(dot3(b,b))))
#define sin(a,b) (sqrt(1-cos(a,b)*cos(a,b)))
#define tan(a,b) (sin(a,b)/cos(a,b))
#define max(a,b) ((a)>(b)?(a):(b))
void normalize3(double *a){
	double tmp=sqrt(dot3(a,a));
	if(tmp>=-1e-14 && tmp<=1e-14){
		double a1[3];
		a1[0]=a[0]*10000000.0;
		a1[1]=a[1]*10000000.0;
		a1[2]=a[2]*10000000.0;
		tmp=sqrt(dot3(a,a));
		if(tmp>=-1e-14 && tmp<=1e-14)return;
		a[0]=a1[0]/tmp;
		a[1]=a1[1]/tmp;
		a[2]=a1[2]/tmp;
		return;
	}
	a[0]=a[0]/tmp;
	a[1]=a[1]/tmp;
	a[2]=a[2]/tmp;
}
static inline int insect_plane(int idx,const objects_p ray,double *t){
	if(objs[idx].obj_type!=OBJ_PLANE)return 0;
	double k=dot3(objs[idx].p,ray->p);
	double tmp3[3];
	sub3(objs[idx].origin,ray->origin,tmp3);
	double b=dot3(tmp3,objs[idx].p);
	(*t)=b/k;
	if((*t)>=1e-6)
		return 1;
	else
		return 0;
}
#define N_IS
#ifdef N_IS
static inline int insect_sphere(int idx,const objects_p ray,double *t){
	if(objs[idx].obj_type!=OBJ_SPHERES)return 0;
	double tmp3[3];
	sub3(objs[idx].origin,ray->origin,tmp3);
	double tmp_1=dot3(tmp3,ray->p);
	double tmp_2=dot3(tmp3,tmp3);
	double tmp_3=tmp_2-tmp_1*tmp_1;
	if(tmp_3>=objs[idx].p[0]*objs[idx].p[0]-1e-8)return 0;
	double tmp_4=tmp_1-sqrt(objs[idx].p[0]*objs[idx].p[0]-tmp_3);
	(*t)=tmp_4;
	if(*t>1e-3)return 1;
	(*t)=2.0*tmp_1-tmp_4;
	if(*t>1e-3)return 1;
	else return 0;
}
#else
static inline int insect_sphere(int idx, const objects_p ray, double* t)
{
	double tmp[3];
	sub3(ray->origin, objs[idx].origin, tmp);
	double b = 2 * dot3(ray->p, tmp);
	double c = dot3(tmp, tmp) - (objs[idx].p[0] * objs[idx].p[0]);
	double disc = b * b - 4 * c;

	if (disc < 0)
		return 0;

	double distSqrt = sqrtf(disc);
	double q;
	if (b < 0)
		q = (-b - distSqrt)/2.0;
	else
		q = (-b + distSqrt)/2.0;

	double t0 = q;
	double t1 = c / q;

	if (t0 > t1){
		double temp = t0;
		t0 = t1;
		t1 = temp;
	}

	if (t1 < 1e-1)
		return 0;

	if (t0 < 1e-1)
		*t = t1;
	else
		*t = t0;
	return 1;
}
#endif
void get_insection(const objects_p ray,double *t,int *idx){
	int i;
	double mint=1e99;
	*idx=-1;
	for(i=0;i<obj_count;i++){
		double tmp;
		int ifinsect=0;
		switch(objs[i].obj_type){
			case OBJ_SPHERES:
				ifinsect=insect_sphere(i,ray,&tmp);
				break;
			case OBJ_PLANE:
				ifinsect=insect_plane(i,ray,&tmp);
				break;
			default:break;
		}
		if(ifinsect && mint>tmp){
			mint=tmp;
			(*idx)=i;
		}
	}
	(*t)=mint;
}
static inline double get_refraction_ratio_sphere(int idx,const objects_p ray){
	double tmp[3];
	sub3(objs[idx].origin, ray->origin, tmp);
	double t2=dot3(tmp, tmp);
	if(t2>objs[idx].p[0]*objs[idx].p[0]+1e-5)
		return objs[idx].in_v/objs[idx].out_v;
	else
		return objs[idx].out_v/objs[idx].in_v;
}
static inline double get_refraction_ratio_plane(int idx,const objects_p ray){
	double tmp[3];
	sub3(objs[idx].origin, ray->origin, tmp);
	double t2=dot3(tmp, objs[idx].p);
	if(t2>0)
		return objs[idx].in_v/objs[idx].out_v;
	else
		return objs[idx].out_v/objs[idx].in_v;
}
double get_refraction_ratio(int idx,const objects_p ray){
	switch(objs[idx].obj_type){
		case OBJ_SPHERES:
			return get_refraction_ratio_sphere(idx,ray);
		case OBJ_PLANE:
			return get_refraction_ratio_plane(idx,ray);
		default:
			return 1;
	}
}
static inline void _reflect_p(const objects_p ray, const objects_p normal, objects_p reflect_ray){
	double tmp_1[3];
	double tmp=dot3(ray->p, normal->p);
	mul3(normal->p, tmp, tmp_1);
	double tmp_2[3];
	sub3(ray->p, tmp_1, tmp_2);
	mul3(tmp_1, -1, tmp_1);
	as(reflect_ray->p, tmp_1);
	add3(reflect_ray->p, tmp_2, reflect_ray->p);
}
static inline double _refract_p(const objects_p ray, const objects_p normal, objects_p refract_ray, objects_p reflect_ray, double ratio){
	double tmp_1[3];
	double tmp=dot3(ray->p, normal->p);
	mul3(normal->p, tmp, tmp_1);
	double tmp_2[3];
	sub3(ray->p, tmp_1, tmp_2);
	double ssina=dot3(tmp_2, tmp_2);
	double ssinb=ssina*(ratio*ratio);
	if(ssinb<=1){
		normalize3(tmp_2);
		normalize3(tmp_1);
		double rr=sqrt(ssinb);
		mul3(tmp_2, rr, tmp_2);
		rr=sqrt(1-ssinb);
		mul3(tmp_1, rr, tmp_1);
		as(refract_ray->p, tmp_1);
		add3(refract_ray->p, tmp_2, refract_ray->p);
		return 1;
	}else{
		_reflect_p(ray, normal, reflect_ray);
		return 0;
	}
}
static inline void get_sphere_normal(int idx, const objects_p insection, objects_p normal){
	sub3(insection->origin, objs[idx].origin, normal->p);
	normalize3(normal->p);
}
static inline void get_plane_normal(int idx, const objects_p insection, objects_p normal){
	as(normal->p, objs[idx].p);
	normalize3(normal->p);
}
void get_normal(int idx, const objects_p insection, objects_p normal){
	memset(normal, 0, sizeof(struct objects));
	if(objs[idx].obj_type==OBJ_SPHERES)
		get_sphere_normal(idx, insection, normal);
	else
		get_plane_normal(idx, insection, normal);
}
double get_refraction(const objects_p ray, int idx, const objects_p insection,
		objects_p refract_ray, objects_p reflect_ray, double ratio){
	struct objects normal;
	get_normal(idx, insection, &normal);
	double ret = _refract_p(ray, &normal, refract_ray, reflect_ray, ratio);
	if(ret > 0)
		as(refract_ray->origin, insection->origin);
	if(ret < 1)
		as(reflect_ray->origin, insection->origin);
	return ret;
}
void get_reflection2(const objects_p ray,const objects_p norm, const objects_p insection,
		objects_p reflect_ray){
	_reflect_p(ray, norm, reflect_ray);
	as(reflect_ray->origin, insection->origin);
}
void get_reflection(const objects_p ray,int idx,const objects_p insection,
		objects_p reflect_ray){
	struct objects normal;
	get_normal(idx, insection, &normal);
	_reflect_p(ray, &normal, reflect_ray);
	as(reflect_ray->origin, insection->origin);
}
double get_lightning(struct objects *insect, struct objects *norm, struct objects *ray, double *total_l){
	int i;double light=0,total_light=0;
	double t3 = dot3(norm->p, ray->p);
	for(i=0;i<light_source_count;i++){
		total_light += light_source[i].light;
		struct objects tobj, tobj2;
		as(tobj.origin, insect->origin);
		sub3(light_source[i].origin, insect->origin, tobj.p);
		get_reflection2(&tobj, norm, insect, &tobj2);
		double tmpl = dot3(tobj.p, tobj.p);
		normalize3(tobj.p);
		if(dot3(tobj.p, norm->p)==0)continue;
		double tmp;int idx;
		get_insection(&tobj, &tmp, &idx);
		if(idx==-1||tmp*tmp>tmpl-(1e-6)){
			double t1 = dot3(norm->p, tobj.p);
			if(t1*t3<1e-10)
				light+=light_source[i].light*abs(cos2(tobj2.p, norm->p));
		}
	}
	if(total_l)*total_l = total_light;
	return light;
}
void ray_trace(const objects_p ray, double *color , double factor){
		//double **save, int save_size){
	double t;int idx;
	if(factor<=1e-4)return;
	get_insection(ray, &t, &idx);
	if(idx<0)return;
	double tmp_1[3];
	struct objects insection,normal;
	memset(&insection, 0 ,sizeof(insection));
	memset(&normal, 0 ,sizeof(normal));
	mul3(ray->p, t, tmp_1);
	add3(ray->origin, tmp_1, insection.origin);
	//save[save_size]=insection.origin;
	get_normal(idx, &insection, &normal);
	double color2[3];
	color2[0]=color2[1]=color2[2]=0;
	if(objs[idx].surface_type!=SURFACE_TRANSPARENT){
		double light, total_light;
		light = get_lightning(&insection, &normal, ray, &total_light);
		double f2=(light/total_light)*(1-objs[idx].reflectivity);
		mul3(objs[idx].color, f2, color2);
	}
	if(objs[idx].surface_type==SURFACE_MIRROR){
		//Deal with reflections
		struct objects reflect_ray;
		get_reflection(ray, idx, &insection, &reflect_ray);
		ray_trace(&reflect_ray, color, factor*objs[idx].reflectivity);//, save, save_size-1);
		mul3(color, objs[idx].reflectivity, color);
		add3(color2, color, color);
	}else if(objs[idx].surface_type==SURFACE_TRANSPARENT){
		//Refraction
		struct objects refract_ray, reflect_ray;
		double ratio=get_refraction_ratio(idx, ray);
		double r2=get_refraction(ray, idx, &insection, &refract_ray, &reflect_ray, ratio);
		ray_trace(&refract_ray, color, factor*r2);
		mul3(color, r2, color);
		add3(color2, color, color2);
		color[0]=color[1]=color[2]=0;
		ray_trace(&reflect_ray, color, factor*(1-r2));
		add3(color2, color, color);
	}
}
int add_sphere(const double *origin, double r, const double *color, double reflectivity,
		surface_type_t st,double in_v, double out_v){
	as(objs[obj_count].origin, origin);
	objs[obj_count].p[0]=r;
	objs[obj_count].p[1]=objs[obj_count].p[2]=0;
	as(objs[obj_count].color, color);
	objs[obj_count].reflectivity=reflectivity;
	objs[obj_count].surface_type=st;
	objs[obj_count].in_v=in_v;
	objs[obj_count].out_v=out_v;
	objs[obj_count].obj_type=OBJ_SPHERES;
	return obj_count++;
}
void set_sphere(int id, const double *origin, double r, const double *color, double reflectivity,
		surface_type_t st, double in_v, double out_v){
	as(objs[id].origin, origin);
	objs[id].p[0]=r;
	objs[id].p[1]=objs[id].p[2]=0;
	as(objs[id].color, color);
	objs[id].reflectivity=reflectivity;
	objs[id].surface_type=st;
	objs[obj_count].in_v=in_v;
	objs[obj_count].out_v=out_v;

}
int add_plane(const double *origin, const double *p, const double *color, double reflectivity,
		surface_type_t st, double in_v, double out_v){
	as(objs[obj_count].origin, origin);
	as(objs[obj_count].p, p);
	normalize3(objs[obj_count].p);
	as(objs[obj_count].color, color);
	objs[obj_count].reflectivity=reflectivity;
	objs[obj_count].surface_type=st;
	objs[obj_count].in_v=in_v;
	objs[obj_count].out_v=out_v;
	objs[obj_count].obj_type=OBJ_PLANE;
	return obj_count++;
}
int add_light(const double *origin, double light){
	as(light_source[light_source_count].origin, origin);
	light_source[light_source_count].light=light;
	light_source[light_source_count].obj_type=OBJ_SOURCE;
	return light_source_count++;
}
void draw_pixel(double x, double y, double eye, double *color){
	struct objects ray;
	memset(&ray, 0 ,sizeof(ray));
	ray.p[0]=x;
	ray.p[1]=y;
//	ray.p[2]=(1-cos(2.0*x))*0.5+(1-cos(2.0*y))*cos(2.0*x)*0.5-eye;
	ray.p[2]=-eye;
	normalize3(ray.p);
	ray.origin[0]=ray.origin[1]=0;
	ray.origin[2]=eye;
	color[0]=color[1]=color[2]=0;
	ray_trace(&ray, color, 1);
}
