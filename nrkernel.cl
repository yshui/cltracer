//Navie Ray Tracing without any accelarete structs
#define eps 1e-3
#define XR 200
#define YR 200
#define ZR 200
inline float insect_plane(float4 op, float4 oo, float4 origin,float4 p){
	float k=dot(op,p);
	float4 tmp3 = oo - origin;
	float b=dot(tmp3,op);
	////printf("%f,%f,%f %f,%f,%f %f,%f,%f\n",p.x,p.y,p.z,tmp3.x,tmp3.y,tmp3.z,origin.x,origin.y,origin.z);
	return b/k;
}
inline float insect_sphere(float op, float4 oo, float4 origin, float4 p){
	float4 tmp3 = oo - origin;
	float tmp_1=dot(tmp3,p);
	float tmp_3=tmp_1*tmp_1+op*op-dot(tmp3, tmp3);
	if(tmp_3<=eps)return -1e10;
	float tmp_4=sqrt(tmp_3);
	////printf("%f %f %f %f,%f,%f\n", tmp_1, op*op-dot(tmp3, tmp3), tmp_4, p.x, p.y, p.z);
	float t1=tmp_1-tmp_4;
	float t2=tmp_1+tmp_4;
	if(t1>eps)
		return t1;
	else if(t2>eps)
		return t2;
	else
		return -1e10;
}
inline float get_sinsection(float4 p, float4 origin, int scount, __constant float4 *so, __constant float *sr, int *idx){
	int i;
	float mint=1e10;
	*idx=-1;
	for(i=0;i<scount;i++){
		float tmp;
		tmp=insect_sphere(sr[i], so[i], origin, p);
		if(tmp > eps && mint>tmp){
			mint=tmp;
			(*idx)=i;
		}
	}
	return mint;
}
inline float get_pinsection(float4 p, float4 origin, int pcount, __constant float4 *po, __constant float4 *pn, int *idx){
	int i;
	float mint=1e10;
	*idx=-1;
	for(i=0;i<pcount;i++){
		float tmp;
		tmp=insect_plane(pn[i], po[i], origin, p);
		if(tmp > eps && mint>tmp){
			mint=tmp;
			(*idx)=i;
		}
	}
	return mint;
}
inline float get_insection(float4 p, float4 o, int pc, int sc, __constant float4 *po, __constant float4 *pn,
		__constant float4 *so, __constant float *sr, int *idx, int *type){
	float mint=1e9;
	int tidx=-1;
	*type=0;
	mint = get_pinsection(p, o, pc, po, pn, idx);
	//printf("pinsect %d\n", get_global_id(0));
	float m2 = get_sinsection(p, o, sc, so, sr, &tidx);
	//printf("sinsect %d\n", get_global_id(0));
	if(m2<mint){
		mint=m2;
		*idx=tidx;
		*type=1;
	}return mint;
}
inline float4 get_normal(int idx, int type, float4 io, __constant float4 *so, __constant float4 *pn){
	return normalize(type*(io-so[idx])+(1-type)*pn[idx]);
}
inline void get_reflection(float4 np, float4 p, float4 o,
		float4 io, float4 *ro, float4 *rp){
	*rp = p-2*dot(p,np)*np;
	*ro = io;
}
inline float get_lightning(float4 io, float4 np, float4 p, int lidx,
		int pc, int sc,
		__constant float4 *po, __constant float4 *pn,
		__constant float4 *so, __constant float *sr, int tty
		){
	int i;float light=0;
	float t3 = dot(np, p);
	for(i=lidx;i<sc;i++){
		float4 tp = so[i]-io;
		float4 rtp, rto;
		float4 to = io;
		float tmpl = dot(tp, tp);
		tp = normalize(tp);
		float tmp;int idx, type;
		tmp = get_insection(tp, to, pc, sc, po, pn, so, sr, &idx, &type);
		get_reflection(np, -tp, to, io, &rto, &rtp);
		if(idx==-1||tmp*tmp>tmpl-eps||idx==i){
			float t1 = dot(np, tp);
			if(t1*t3<1e-10){
				////printf("%f\n", sqrt(dot(np,np)));
				light+=fabs(t1)*0.8*(1+max(0.0f, dot(rtp, -p)));
			}
		}
	}
	return light/(float)(sc-lidx);
}
inline int grid_index(float x, float y, float z){
	int ix=floor(x)+XR/2, iy=floor(y)+YR/2, iz=floor(z)+ZR/2;
	return (ix*YR+iy)*ZR+iz;
}
#define PHOTON_R 0.1f
inline float4 summ_photon(float4 io, __constant int *grid, __global int *gridc, __constant float8 *photon){
	float i,j,k;
	float4 clr=0;
	if(io.x<=-100 || io.x>=100 || io.y<=-100 || io.y>=100 || io.z <= -100 || io.z >=100)
		return clr;
	for(i=io.x-1;i<io.x+1.5;i+=1)
	for(j=io.y-1;j<io.y+1.5;j+=1)
	for(k=io.z-1;k<io.z+1.5;k+=1){
		int id=grid_index(i,j,k);
		int l;
		for(l=gridc[id];l<gridc[id+1];l++){
			float4 tmp = photon[l].xyzw-io;
			float dist = dot(tmp, tmp);
			clr+=photon[l].s4567*max(0.0f, 1.0f-sqrt(dist)/PHOTON_R);
		}
	}
	return clr;
}
#define RTDEPTH 1000000
inline float4 ray_trace(float4 p, float4 o, int lidx,
	int pc, int sc,
	__constant float4 *po, __constant float4 *pn,
	__constant float4 *so, __constant float *sr,
	__constant float *ore, __constant float4 *oc,
	__constant int *grid, __global int *gridc,
	__constant float8 *photon){
	float t;int idx=-1,type=0;
	float factor = 1;
	float4 trp = p;
	float4 tro = o;
	float4 color = 0;
	int depth = 0;
	while(1){
		//printf("iter %d\n", depth);
		if(depth>RTDEPTH)break;
		if(factor<=1e-4)return color;
		t=get_insection(trp,tro,pc,sc,po,pn,so,sr,&idx,&type);
		//printf("insect\n");
		if(idx<0)return color;
		float tmp_1[3];
		float4 io = tro+trp*t;
		float4 np=get_normal(idx, type,  io, so, pn);
		int idx2=pc*type+idx;
		float light = get_lightning(io, np, trp, lidx, pc, sc, po, pn, so, sr, type);
		//printf("lightning\n");
		float f2=light*(1-ore[idx2])*factor;
		float4 toc;
		toc=oc[idx2]*f2;
		color=color+toc;
		//printf("summon start\n");
		color=color+summ_photon(io, grid, gridc, photon)*factor;
		//printf("summon\n");
		//Deal with reflections
		get_reflection(np, trp, tro, io, &tro, &trp);
		factor *= ore[idx2];
		depth++;
	}
	return color;
}
__kernel void work(uint lidx, uint pc, uint sc,
	__constant float4 *po, __constant float4 *pn, 
	__constant float4 *so, __constant float *sr,
	__constant float4 *oc, __constant float *ore,
	__constant int *grid, __global int *gridc,
	__constant float8 *photon, __write_only image2d_t out){
	int2 cor;
	cor.x=get_global_id(0);
	cor.y=get_global_id(1);
	//printf("%d, %d started\n", cor.x, cor.y);
	float4 o, p;
	p.x=((float)cor.x)/(float)480-0.5;
	p.y=1;
	p.z=((float)cor.y)/(float)480-0.5;
	p.w=0;
	p = normalize(p);
	o.x=o.z=o.w=0;
	o.y=-1;
	float4 clr=0;
	clr = ray_trace(p, o, lidx, pc, sc, po, pn, so, sr, ore, oc,grid,gridc,photon);
	write_imagef(out, cor, clr);
}
__kernel void motion(__global float4 *so, float delta_time, float time){
	so[0].x = so[0].x - sin(time) + sin(time+delta_time);
	so[0].z = so[0].z - cos(time) + cos(time+delta_time);
}
__kernel void photon_placement(__global float4 *photon, __global int *gridc, __global int *grid, __global int *tmpg){
	int i, tmp=gridc[0];
	gridc[0]=0.0;
	//printf("start\n");
	for(i=1; i<=XR*YR*ZR;i++){
		int tmp2 = gridc[i];
		gridc[i]=gridc[i-1]+tmp;
		tmpg[i]=gridc[i];
		tmp=tmp2;
	}
	//printf("phase 1\n");
	int phm=gridc[XR*YR*ZR];
	for(i=0;i<phm;i++){
		int id=grid_index(photon[i].x, photon[i].y, photon[i].z);
		if(photon[i].w == 0)
			grid[tmpg[id]++]=i;
	}
	//printf("phase 2 %d\n", phm);
}
inline int photon_trace(float4 p, float4 o, int pc, int sc,
	__constant float4 *po, __constant float4 *pn,
	__constant float4 *so, __constant float *sr,
	__constant float *ore, __constant float4 *oc,
	__global float8 *photon, __global float *rand){
	float t;int idx=-1,type=0;
	float factor = 1;
	float4 trp = p;
	float4 tro = o;
	float4 trc = 1;
	int depth = 0;
	int randi = get_global_id(0);
	//printf("trace %d started\n", randi);
	while(1){
		//printf("iter s %d\n", depth);
		if(depth>RTDEPTH)break;
		//printf("before insect\n");
		get_insection(trp,tro,pc,sc,po,pn,so,sr,&idx,&type);
		//printf("insect %d\n", idx);
		if(idx<0)//printf("trace %d ended with %d iter B\n", get_global_id(0), depth);
		if(idx<0){
			(*photon)=0;
			(*photon).w = 1;
			return 0;
		}
		float4 io = tro+trp*t;
		float4 np=get_normal(idx, type,  io, so, pn);
		//printf("normal\n");
		int idx2=pc*type+idx;
		if(rand[randi++] > ore[idx2])break;
		//printf("%d\n", randi);
		trc=trc*oc[idx2];
		//printf("trc\n");
		//Deal with reflections
		get_reflection(np, trp, tro, io, &tro, &trp);
		depth++;
	}
	//printf("trace %d ended with %d iter\n", get_global_id(0), depth);
	tro.w=0;
	trc.w=0;
	*photon = (float8)(tro, trc);
	return 1;
}
__kernel void photon_generate(int pc, int sc, int lidx,
			__constant float4 *po, __constant float4 *pn,
			__constant float4 *so, __constant float *sr,
			__constant float4 *oc, __constant float *ore,
			__global float8 *photon, __global int *gridc,
			__global float4 *pho, __global float4 *php,
			__global float *rand){
	int phm=get_global_id(0);
	int ret=photon_trace(php[phm], pho[phm], 
				pc, sc, 
				po, pn, 
				so, sr, 
				ore, oc, 
				photon+phm, rand);
	int id=grid_index(photon[phm].x, photon[phm].y, photon[phm].z);
	//printf("before write (%d) %d %f,%f,%f,%f\n", phm, id,photon[phm].x, photon[phm].y, photon[phm].z,photon[phm].w);
	atomic_add(gridc+id, ret);
	//printf("done write (%d) %d\n",phm, gridc[id]);
}
__kernel void setup_grid(__global int *gridc){
	gridc[get_global_id(0)]=0;
}
