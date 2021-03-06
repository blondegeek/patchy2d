#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <immintrin.h>
#include <utils.h>
#include <string.h>
#include <mpi.h>
#include <signal.h>
#include <time.h>
#include "dSFMT.h"
#include "params.h"
#include "zargs.h"
#include "hash.h"
#include "mm_math.h"
#include "patches.h"
#include "energy.h"
#include "lists.h"
#include "alloc.h"
#include "graph.h"
#include "canonical.h"
#include "optimize.h"
#include "init.h"
//#include "postscript.h"
extern dsfmt_t dsfmt;
void compress(header *t,double xnew){
	unsigned int i;
	compound_particle *c;
	particle *p;
	for(i=0;i<t->nparticle;i++){
		p=t->p+i;	
		*(p)->q_tmp=*(p)->q;
		list_swap(p);
	}
	for(i=0;i<t->ncompound;i++){
		c=t->c+i;	
		*(c)->q_tmp=*(c)->q;
		*(c)->q*=xnew;
		reset_particle(c,t);
	}
}
void compress_m128d(header *t,__m128d f){
	unsigned int i;
	compound_particle *c;
	particle *p;
	for(i=0;i<t->nparticle;i++){
		p=t->p+i;	
		*(p)->q_tmp=*(p)->q;
		list_swap(p);
	}
	for(i=0;i<t->ncompound;i++){
		c=t->c+i;	
		*(c)->q_tmp=*(c)->q;
		*(c)->q*=f;
		reset_particle(c,t);
	}
}
void compress_reset(header *t){
	unsigned int i;
	compound_particle *c;
	particle *p;
	for(i=0;i<t->ncompound;i++){
		c=t->c+i;	
		*(c)->q=*(c)->q_tmp;
	}
	for(i=0;i<t->nparticle;i++){
		p=t->p+i;	
		*(p)->q=*(p)->q_tmp;
		list_swap(p);
		hash_reinsert(p,t->h1,t->table);
	}
}
int mc_npt(header *t,int *en){
	//unsigned int i;
	int enn,eno,de;
	double vol,vol_new,xnew;
	double rnd=dsfmt_genrand_open_open(&dsfmt)-0.5;
	double dv=t->max_vol*rnd*t->nparticle;
	double acc;
	double bp;
	int ennb=0;
	vol=t->box[0]*t->box[1];
	vol_new=vol+dv;
	xnew=sqrt(vol_new/vol);
	__m128d box=t->box;
	eno=*en;
	t->box*=xnew;
	hash1(t);
	hash_lists(t); //FIXME but only if number of the cells changes
	compress(t,xnew);
	ennb=all_particle_energy_hash(t,&enn);
	de=enn-eno;
	bp=t->pressure*fabs(t->epsilon); //betap
	if(ennb!=-1){
		acc=de*t->epsilon-bp*dv+t->ncompound*log(vol_new/vol);
		rnd=dsfmt_genrand_open_open(&dsfmt);
		if(rnd<exp(acc)){
			*en=enn;
			return 0;
		}
	}
	t->box=box;
	hash1(t);
	hash_lists(t);
	compress_reset(t);
	return 1;
}
int mc_npt_xy(header *t,int *en){
	//unsigned int i;
	int enn,eno,de;
	double vol,vol_new;//,xnew;
	double rnd=dsfmt_genrand_open_open(&dsfmt)-0.5;
	double dx=t->max_xy*rnd;
	double dv;
	double acc;
	double bp;
	unsigned int d=(unsigned)(dsfmt_genrand_open_open(&dsfmt)*2);
	int ennb=0;
	//species *s;
	//particle *q;
	__m128d box=t->box;
	__m128d f;
	eno=*en;
	vol=t->box[0]*t->box[1];
	if(t->box[d]+dx<3*t->hash_cell[d]){
		return 1;
	}
	t->box[d]+=dx;
	f=t->box/box;
	vol_new=t->box[0]*t->box[1];
	dv=vol_new-vol;
	hash1(t);
	hash_lists(t); //FIXME but only if number of the cells changes
	/*s=t->specie;
	while(s){
		for(i=0;i<s->nparticle;i++){
			q=(particle*)s->p+i;
			*(q)->q_tmp=*(q)->q;
			*(q)->q*=f;
			list_swap(q);
			hash_reinsert(q,t->h1,t->table);
		}
		s=s->next;
	}*/
	compress_m128d(t,f);
	ennb=all_particle_energy_hash(t,&enn);
	de=enn-eno;
	bp=t->pressure*fabs(t->epsilon); //betap
	if(ennb!=-1){
		acc=de*t->epsilon-bp*dv+t->nparticle*log(vol_new/vol);
		rnd=dsfmt_genrand_open_open(&dsfmt);
		if(rnd<exp(acc)){
			*en=enn;
			return 0;
		}
	}
	t->box=box;
	hash1(t);
	hash_lists(t);
	compress_reset(t);
	/*s=t->specie;
	while(s){
		for(i=0;i<s->nparticle;i++){
			q=(particle*)s->p+i;
			*(q)->q=*(q)->q_tmp;
			list_swap(q);
			hash_reinsert(q,t->h1,t->table);
		}
		s=s->next;
	}*/
	return 1;
}
int mc_npt_dxdy(header *t,int *en){
	//unsigned int i;
	int enn,eno,de;
	double vol,vol_new;//,xnew;
	double rnd=dsfmt_genrand_open_open(&dsfmt)-0.5;
	double dx=t->max_dxdy*rnd;
	double dv;
	double acc;
	double bp;
	int ennb=0;
	//unsigned int d=(unsigned)(dsfmt_genrand_open_open(&dsfmt)*2);
	//species *s;
	//particle *q;
	__m128d box=t->box;
	__m128d f;
	eno=*en;
	vol=t->box[0]*t->box[1];
	t->box[0]+=dx;
	t->box[1]=vol/t->box[0];
	f=t->box/box;
	vol_new=t->box[0]*t->box[1];
	dv=vol_new-vol;
	hash1(t);
	hash_lists(t); //FIXME but only if number of the cells changes
	/*s=t->specie;
	while(s){
		for(i=0;i<s->nparticle;i++){
			q=(particle*)s->p+i;
			*(q)->q_tmp=*(q)->q;
			*(q)->q*=f;
			list_swap(q);
			hash_reinsert(q,t->h1,t->table);
		}
		s=s->next;
	}*/
	compress_m128d(t,f);
	ennb=all_particle_energy_hash(t,&enn);
	de=enn-eno;
	bp=t->pressure*fabs(t->epsilon); //betap
	if(ennb!=-1){
		acc=de*t->epsilon-bp*dv+t->nparticle*log(vol_new/vol);
		rnd=dsfmt_genrand_open_open(&dsfmt);
		if(rnd<exp(acc)){
			*en=enn;
			return 0;
		}
	}
	t->box=box;
	hash1(t);
	hash_lists(t);
	compress_reset(t);
	/*s=t->specie;
	while(s){
		for(i=0;i<s->nparticle;i++){
			q=(particle*)s->p+i;
			*(q)->q=*(q)->q_tmp;
			list_swap(q);
			hash_reinsert(q,t->h1,t->table);
		}
		s=s->next;
	}*/
	return 1;
}

int mc_uy(header *t,int *en){
	unsigned int i;
	int enn,eno,de;
	double rnd=dsfmt_genrand_open_open(&dsfmt)-0.5;
	double dy=t->max_uy*rnd;
	double acc;
	double uy_old=t->uy;
	int ennb=0;
	species *s;
	particle *q;
	eno=*en;
	t->uy+=dy;
	if(fabs(t->uy)>0.7){
		t->uy=uy_old;
		return 1;
	}
	s=t->specie;
	while(s){
		for(i=0;i<s->nparticle;i++){
			q=(particle*)s->p+i;
			list_swap(q);
			hash_reinsert(q,t->h1,t->table);
		}
		s=s->next;
	}
	ennb=all_particle_energy_hash(t,&enn);
	de=enn-eno;
	if(ennb!=-1){
		acc=de*t->epsilon;
		rnd=dsfmt_genrand_open_open(&dsfmt);
		if(rnd<exp(acc)){
			*en=enn;
			return 0;
		}
	}
	t->uy=uy_old;
	s=t->specie;
	while(s){
		for(i=0;i<s->nparticle;i++){
			q=(particle*)s->p+i;
			list_swap(q);
			hash_reinsert(q,t->h1,t->table);
		}
		s=s->next;
	}
	return 1;
}
