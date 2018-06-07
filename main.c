#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#define GL_GLEXT_PROTOTYPES
#include <utils.h>
#include "dSFMT.h"
#include "program_gl.h"
#include "mySDL.h"
#include "init.h"
#include "run.h"
#define MAX_SOURCE_SIZE 8096
dsfmt_t dsfmt;
int main(int argc, char *argv[]){
	//Initialize header
	header *t=init_header();
	dsfmt_init_gen_rand(&dsfmt,t->seed);
	//Reading inputs
	if(argc==1){
		usage(stdout,t->argz,*argv);
		return 0;
	}
	printf(">>>Initialized default header\n");
	input_files *input=find_configurational_files(argc,argv);
	read_input(argc,argv,input,t);
	
	dump_args(stdout,t->argz);

	//Graphics
	float color[4]={0.5,1.0,0.5,1.0};
	mySDL *s=mySDLinit();

	s->positions=alloc(sizeof(float)*2*t->Nalloc);
	s->colors=alloc(sizeof(float)*4*t->Nalloc);
	s->n=t->N;
	m128d2float(t->p->q,s->positions,s->n);
	mySDLsetcolor(s->colors,color,s->n);

	s->box=(float[8]){0.0,0.0,t->box[0],0.0,t->box[0],t->box[1],0.0,t->box[1]};
	s->scale=s->box[4]/(s->box[4]+1.0);

	mySDLresize(s);
	mySDLpositions(s,s->positions,s->n);
	mySDLcolors(s,s->colors,s->n);
	mySDLboundary(s,s->box);

	mySDLdisplay(s);

	run(t,s);

	SDL_Quit();

	save_configuration(t->name,t);

	return 0;
}
