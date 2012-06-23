//Navie Ray Tracing without any accelarete structs
#define eps 1e-3
inline float insect_plane(float4 op, float4 oo, float4 origin,float4 p){
	float k=dot(op,p);
	float4 tmp3 = oo - origin;
	float b=dot(tmp3,op);
	//printf("%f,%f,%f %f,%f,%f %f,%f,%f\n",p.x,p.y,p.z,tmp3.x,tmp3.y,tmp3.z,origin.x,origin.y,origin.z);
	return b/k;
}
inline float insect_sphere(float op, float4 oo, float4 origin, float4 p){
	float4 tmp3 = oo - origin;
	float tmp_1=dot(tmp3,p);
	float tmp_3=tmp_1*tmp_1+op*op-dot(tmp3, tmp3);
	if(tmp_3<=eps)return -1e10;
	float tmp_4=sqrt(tmp_3);
	//printf("%f %f %f %f,%f,%f\n", tmp_1, op*op-dot(tmp3, tmp3), tmp_4, p.x, p.y, p.z);
	float t1=tmp_1-tmp_4;
	float t2=tmp_1+tmp_4;
	if(t1>eps)
		return t1;
	else if(t2>eps)
		return t2;
	else
		return -1e10;
}
float get_sinsection(float4 p, float4 origin, int scount, __constant float4 *so, __constant float *sr, int *idx){
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
float get_pinsection(float4 p, float4 origin, int pcount, __constant float4 *po, __constant float4 *pn, int *idx){
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
float get_insection(float4 p, float4 o, int pc, int sc, __constant float4 *po, __constant float4 *pn,
		__constant float4 *so, __constant float *sr, int *idx, int *type){
	float mint=1e9;
	int tidx=-1;
	*type=0;
	mint = get_pinsection(p, o, pc, po, pn, idx);
	float m2 = get_sinsection(p, o, sc, so, sr, &tidx);
	if(m2<mint){
		mint=m2;
		*idx=tidx;
		*type=1;
	}return mint;
}
inline float4 get_normal(int idx, int type, float4 io, __constant float4 *so, __constant float4 *pn){
	return normalize(type*(io-so[idx])+(1-type)*pn[idx]);
}
void get_reflection(float4 np, float4 p, float4 o,
		float4 io, float4 *ro, float4 *rp){
	*rp = p-2*dot(p,np)*np;
	*ro = io;
}
float get_lightning(float4 io, float4 np, float4 p, float4 o, int lc, __constant float *ls, __constant float4 *lso,
		int pc, int sc,
		__constant float4 *po, __constant float4 *pn,
		__constant float4 *so, __constant float *sr, int tty){
	int i;float light=0,total_l=0;
	float t3 = dot(np, p);
	for(i=0;i<lc;i++){
		total_l += ls[i];
		float4 tp = lso[i]-io;
		float4 to = io;
		float tmpl = dot(tp, tp);
		tp = normalize(tp);
		float tmp;int idx, type;
		tmp = get_insection(tp, to, pc, sc, po, pn, so, sr, &idx, &type);
		get_reflection(np, -tp, to, io, &rto, &rtp);
		//if(tty == 1){
		//	printf("%d %d %f\n",idx,type,tmp);
		//}
		if(idx==-1||tmp*tmp>tmpl-eps){
			float t1 = dot(np, tp);
			if(t1*t3<1e-10){
				//printf("%f\n", sqrt(dot(np,np)));
				light+=ls[i]*fabs(dot(tp,np)/sqrt(dot(tp,tp))/sqrt(dot(np,np)));
			}
		}
	}
	return light/total_l;
}
#define RTDEPTH 1000000
float4 ray_trace(float4 p, float4 o, int pc, int sc, int lc,
	__constant float4 *po, __constant float4 *pn,
	__constant float4 *so, __constant float *sr,
	__constant float *ore, __constant float4 *oc,
	__constant float *ls, __constant float4 *lso){
	float t;int idx=-1,type=0;
	float factor = 1;
	float4 trp = p;
	float4 tro = o;
	float4 color = 0;
	int depth = 0;
	while(1){
		if(depth>RTDEPTH)break;
		if(factor<=1e-4)return color;
		t=get_insection(trp,tro,pc,sc,po,pn,so,sr,&idx,&type);
		if(idx<0)return color;
		float tmp_1[3];
		float4 io = tro+trp*t;
		float4 np=get_normal(idx, type,  io, so, pn);
		int idx2=pc*type+idx;
		float light = get_lightning(io, np, trp, tro, lc, ls, lso, pc, sc, po, pn, so, sr, type);
		float f2=light*(1-ore[idx2])*factor;
		float4 toc;
		//if(type==0)
		//	toc=oc[idx2]*f2*(sin(io.x+io.y+io.z)>0);
		//else
			toc=oc[idx2]*f2;
		color=color+toc;
		//Deal with reflections
		get_reflection(np, trp, tro, io, &tro, &trp);
		factor *= ore[idx2];
		depth++;
	}
	return color;
}
__kernel void work(uint pc, uint sc, uint lc,
	__constant float4 *po, __constant float4 *pn, 
	__constant float4 *so, __constant float *sr,
	__constant float *ls, __constant float4 *lso,
	__constant float4 *oc, __constant float *ore, __write_only image2d_t out){
	int2 cor;
	cor.x=get_global_id(0);
	cor.y=get_global_id(1);
	float4 o, p;
	p.x=((float)cor.x)/(float)480-0.5;
	p.y=1;
	p.z=((float)cor.y)/(float)480-0.5;
	p.w=0;
	p = normalize(p);
	o.x=o.z=o.w=0;
	o.y=-1;
	float4 clr=0;
	clr = ray_trace(p, o, pc, sc, lc, po, pn, so, sr, ore, oc, ls, lso);
	write_imagef(out, cor, clr);
}
__kernel void motion(__global float4 *so, float delta_time, float time){
	so[0].x = so[0].x - sin(time) + sin(time+delta_time);
	so[0].z = so[0].z - cos(time) + cos(time+delta_time);
}
__kernel void photon_generate(__global float4 *lsc, __global float4 *lso, __global float4 *lsr, __global float4 *photon, __global int *gridc){
	int idx=get_global_id(0);
}
