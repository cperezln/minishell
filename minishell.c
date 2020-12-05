#include <stdio.h>
#include <unistd.h>
#include "parser.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

int
main(void) {
	int status; //variable para que el padre espere
	int p[2]; //Puntero a pipe
	char buf[1024];
	tline * line;
	int i,j, fdIn, fdOut, fdErr;
	char directorioActual[512];
	FILE * newInput, *newOutput, *newErr; 
	int boolIn, boolOut, boolErr;
	printf("%s==> ",getcwd(directorioActual,-1));	
	while (fgets(buf, 1024, stdin)) {
		boolIn = 0;
		boolOut = 0;
		boolErr = 0;
		line = tokenize(buf);
		if (line==NULL) {
			continue;
		}
		if (line->redirect_input != NULL) {
			printf("redirección de entrada: %s\n", line->redirect_input);
			boolIn = 1; //declaramos una variable booleana para distinguir casos a continuación
		}
		if (line->redirect_output != NULL) {
			printf("redirección de salida: %s\n", line->redirect_output);
			boolOut = 1;
		}
		if (line->redirect_error != NULL) {
			printf("redirección de error: %s\n", line->redirect_error);
			boolErr = 1;
		}
		if (line->background) {
			printf("comando a ejecutarse en background\n");
		} 
		if (line->ncommands == 1) { // Diferenciar caso un único mandato
			int pid = fork();
			if (pid == 0) {
				if (boolIn) { //si hay red. de entrada
					newInput = fopen(line-> redirect_input, "r"); //abrimos el fichero como escritura
					fdIn = fileno(newInput);
					dup2(fdIn, 0);
					fclose(newInput);
				}
				if (boolOut) { //si hay red. de salida
					newOutput = fopen(line-> redirect_output, "w+"); //abrimos el fichero como escritura
					fdOut= fileno(newOutput);
					dup2(fdOut, 1);
					fclose(newOutput);
				}
				/*if (boolErr) { //si hay red. de salida
					newErr = fopen(line-> redirect_error, "w+"); //abrimos el fichero como escritura
					fdErr= fileno(newErr);
					dup2(fdErr, 2);
					fclose(newErr);
				}*/
				execvp(line->commands[0].filename, line->commands[0].argv);
				 // DUDA: ¿cuándo finaliza el programa, lo mata?
				exit(1);
			}
			else{
				wait(&status);
			}
		}
		else{
			char bufAux[1024];
			pipe(p);
			int pid1 = fork(); //primer hijo
			if (pid1 == 0) {
				if (boolIn) { //si hay red. de entrada
					newInput = fopen(line-> redirect_input, "r"); //abrimos el fichero como escritura
					fdIn = fileno(newInput);
					dup2(fdIn, 0);
					fclose(newInput);
				}
				close(p[0]);
				dup2(p[1], 1); 
				//fprintf(stderr, "Primer hijo\n");
				execvp(line->commands[0].filename, line->commands[0].argv);
				//fprintf(stderr, "Postex");
				exit(1);
			}
			else{
				wait(&status);
				//read(p[0], buf, 1024);
				//fprintf(stderr, "%s\n", buf);
				int pAux[2];
				pipe(pAux);
				for (int i = 1; i < (line->ncommands - 1); i++) {	
					//fprintf(stderr, "For dentro del padre");
					/*read(p[0], buf, 1024);
					write(pAux[1], buf, 1024);*/
					dup2(p[0], pAux[1]);
					int pid2 = fork();
					if (pid2 == 0) {
						//fprintf(stderr, "dentro del hijo2");
						close(pAux[1]);
						close(p[0]);
						dup2(pAux[0], 0);
						dup2(p[1], 1);
						execvp(line->commands[i].filename, line->commands[i].argv);
						exit(1);
					}
					else{
						wait(&status);
					}
				}
				//fprintf(stderr, "padre pero no for\n");
				int pidFin = fork();
				if (pidFin == 0) {
					dup2(p[0], 0);
					if (boolOut) { //si hay red. de salida
						newOutput = fopen(line-> redirect_output, "w+"); //abrimos el fichero como escritura
						fdOut= fileno(newOutput);
						dup2(fdOut, 1);
						fclose(newOutput);
					}
					/*if (boolErr) { //si hay red. de salida
						newErr = fopen(line-> redirect_error, "w+"); //abrimos el fichero como escritura
						fdErr= fileno(newErr);
						dup2(fdErr, 2);
						fclose(newErr);
					}*/
					fprintf(stderr, "Estamos dentro de hijo fin\n");
					close(0);
					execvp(line->commands[line->ncommands-1].filename, line->commands[line->ncommands-1].argv);
					exit(1);
				}
				else{
					fprintf(stderr, "A continuación del wait\n");
					wait(NULL);
				}
			}
			/*for (i=0; i<line->ncommands; i++) {
				printf("orden %d (%s):\n", i, line->commands[i].filename);
				for (j=0; j<line->commands[i].argc; j++) {
					printf("  argumento %d: %s\n", j, line->commands[i].argv[j]);
				}
			}*/
		}
		printf("%s==> ",getcwd(directorioActual,-1));	
	}
	return 0;
}
