#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <math.h>
#include <utils.h>
#include <string.h>
#include <mpi.h>
#include <time.h>
#include "dSFMT.h"
#include "zargs.h"
#include "params.h"
#include "mm_math.h"
#include "hash.h"
#include "energy.h"
#include "lists.h"
#include "init.h"
#include "patches.h"
#include "mpi.h"
#include "graph.h"
#include "optimize.h"
#include "canonical.h"

#include <SDL2/SDL.h>
#include <GL/glew.h>
#define GL_GLEXT_PROTOTYPES
#include "mySDL.h"
//#include "postscript.h"
extern dsfmt_t dsfmt;
static volatile sig_atomic_t safe_exit=0;
void signal_safe_exit(int sig){
	safe_exit=1;
	printf(RESET"["RED"terminating"RESET"] [signal %d]\n",sig);
}
void signal_safe_exit_int(int sig){
	safe_exit=1;
	printf("\n ["RED"terminating"RESET"] [signal %d]\n",sig);
	signal(sig,SIG_DFL);
	return;
}
particle *rnd_particle(header *t){
	species *s=t->specie;
	unsigned int rnd=(unsigned)(dsfmt_genrand_open_open(&dsfmt)*t->N);
	while(s&&s->N<(rnd+1)){
		rnd-=s->N;
		s=s->next;
	}
	return (particle*)s->p+rnd;
}
particle *rnd_specie(species *s){
	unsigned int rnd=(unsigned)(dsfmt_genrand_open_open(&dsfmt)*s->N);
	return (particle*)s->p+rnd;
}
int mc_move(particle *p,header *t,int *en){
	int eno=0,enn=0;
	int de;
	double rmd;
	__m128d q0=*(p->q);
	__m128d rnd=(__m128d){dsfmt_genrand_open_open(&dsfmt)-0.5,dsfmt_genrand_open_open(&dsfmt)-0.5};
	__m128d dq=t->max_displacement*rnd;
	dq[0]-=dq[1]*t->uy;
	*(p->q)+=dq;
	boundary(p->q,t->box);
	hash_reinsert(p,t->h1,t->table);
	eno=p->en_new;
	list_swap(p);
	if(overlap(p,t)){
		*(p->q)=q0;
		hash_reinsert(p,t->h1,t->table);
		list_swap(p);
		return 1;
	}
	enn=particle_energy_hash2(p);
	de=enn-eno;
	rmd=dsfmt_genrand_open_open(&dsfmt);
	if(rmd<exp(t->epsilon*de)){
		adjust_lists(p);
		*en+=de;
		return 0;
	}
	else{
		*(p->q)=q0;
		hash_reinsert(p,t->h1,t->table);
		list_swap(p);
		return 1;
	}
}
int mc_rotate(particle *p,header *t,int *en){
	int eno=0,enn=0;
	int de;
	double w=t->max_rotation*(dsfmt_genrand_open_open(&dsfmt)-0.5);
	double rnd=dsfmt_genrand_open_open(&dsfmt);
	__m128d or0=*p->or;
	*p->or=rot2w(*p->or,w);
	eno=p->en_new;
	list_swap(p);
	set_patches(p);
	enn=particle_energy_hash(p,t);
	de=enn-eno;
	rnd=dsfmt_genrand_open_open(&dsfmt);
	if(rnd<exp(t->epsilon*de)){
		adjust_lists(p);
		*en+=de;
		return 0;
	}
	else{
		*(p->or)=or0;
		hash_reinsert(p,t->h1,t->table);
		set_patches(p);
		list_swap(p);
		return 1;
	}
}
void print_nvt_log(FILE *f,header *t,long long int i,double time,int energy,double frac[2]){
	double vol=t->box[0]*t->box[1];
	double rho=(double)t->N/vol;
	fprintf(f,UGREEN"NVT\n"RESET
			CYAN"step:"BLUE" %Ld "RESET
			CYAN"time:"BLUE" %.0lfs "RESET
			CYAN"mod:"BLUE" %d "RESET
			CYAN"pmod:"BLUE" %d \n"RESET
			YELLOW"energy:"RED" %d "RESET
			YELLOW"U/N:"URED" %.3lf "RESET
			YELLOW"rho:"GREEN" %.3lf \n"RESET
			UBLUE"parameters\n"RESET
			BLACK"displacement:"GREEN" %.4lf "RESET
			BLACK"rotation:"GREEN" %.4lf \n"RESET
			UBLUE"acceptance\n"RESET
			BLACK"displacement:"PURPLE" %.2lf "RESET
			BLACK"rotation:"PURPLE" %.2lf \n"RESET,
			i,time,t->mod,t->pmod,
			energy,(double)energy/t->N,rho,
			t->max_displacement[0],t->max_rotation,
			frac[0],frac[1]);
}
int run(header *t,mySDL *s){

	long long int i;
	unsigned int ncycle;
	particle *q;
	int energy=0;

	FILE *fen=open_file2(t->name,".en","w");
	double en;
	time_t t1,t2;

	int acc_move[2]={0,0};
	int acc_rotate[2]={0,0};
	t->max_displacement=_mm_set1_pd(t->max_displacement[0]);

	int *acc[]={acc_move,acc_rotate};
	double *mmax[]={&(t->max_displacement[0]),&t->max_rotation};
	double frac[2];

	signal(SIGINT,signal_safe_exit_int);
	signal(SIGUSR1,signal_safe_exit);
	time(&t1),t2=t1;

	all_particle_energy_hash(t,&energy);
	printf(">>>Running canonical simulation ["RED"%d"RESET"]\n",energy);

	//alloc_graph(t);
	
	for(i=1;!safe_exit&&i<t->step;i++){
		//SDL sindow
		SDL_PollEvent(&s->event);
		switch(s->event.type){
			case SDL_QUIT:
				safe_exit=1;
				break;
			case SDL_WINDOWEVENT:
				if(s->event.window.event==SDL_WINDOWEVENT_RESIZED){
					mySDLresize(s);
				}
				break;
		}

		for(ncycle=0;ncycle<2*t->N;ncycle++){
			q=rnd_particle(t);
			if(0.5<dsfmt_genrand_open_open(&dsfmt)||!q->npatch){
				acc_move[mc_move(q,t,&energy)]++;	
			}
			else{
				acc_rotate[mc_rotate(q,t,&energy)]++;
			}
		}
		if(!(i%(t->mod*t->pmod))){
			time(&t2);
			en=(double)energy/(double)t->N;
			uwrite(&en,sizeof(double),1,fen);
			gfrac(acc,frac,2);
			if(t->optimize){
				optimize(mmax,frac,2);
				t->max_displacement=_mm_set1_pd(t->max_displacement[0]);
			}
			if(t->verbose){
				print_nvt_log(stdout,t,i,difftime(t2,t1),energy,frac);

				m128d2float(t->p->q,s->positions,s->n);
				mySDLpositions(s,s->positions,s->n);
				mySDLcolors(s,s->colors,s->n);
				mySDLboundary(s,s->box);

				mySDLdisplay(s);
			}
		}
	}
	mySDLdisplay(s);
	//find_all_cycles(t);
	close_file(fen);
	checksum(stdout,t,energy);
	return 0;
}