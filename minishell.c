#include <stdio.h>
#include <unistd.h>
#include "parser.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

//Funciones Ejecutar
void exec1Command(int boolIn, int boolOut, int boolErr, int bg, tline* line);
void execNCommand(int boolIn, int boolOut, int boolErr, int bg, tline* line);

//Variables globales
FILE * newInput, *newOutput, *newErr; 
int status;
int fdIn, fdOut, fdErr;
char directorioActual[512];

int main(void) {
	char buf[1024];
	tline * line;
	int boolIn, boolOut, boolErr, bg;
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
			exec1Command(boolIn, boolOut, boolErr, bg, line);
		}
		else if (line->ncommands>1) {
			execNCommand(boolIn, boolOut, boolErr, bg, line);
		}
		printf("%s==> ",getcwd(directorioActual,-1));	
	}
	return 0;
}

void exec1Command(int boolIn, int boolOut, int boolErr, int bg, tline* line){
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
		if (boolErr) { //si hay red. de salida
			newErr = fopen(line-> redirect_error, "w+"); //abrimos el fichero como escritura
			fdErr= fileno(newErr);
			dup2(fdErr, 2);
			fclose(newErr);
		}
		execvp(line->commands[0].filename, line->commands[0].argv);
		exit(1);
	}
	else{
		wait(&status);
	}

}
void execNCommand(int boolIn, int boolOut, int boolErr, int bg, tline* line){
	int i,j;
	char buf[1024];
	int listaPipes[line->ncommands-1][2];
	pipe(listaPipes[0]);
	int pid1 = fork(); //primer hijo
	if (pid1 == 0) {
		close(listaPipes[0][0]);
		if (boolIn) { //si hay red. de entrada
			newInput = fopen(line-> redirect_input, "r"); //abrimos el fichero como escritura
			fdIn = fileno(newInput);
			dup2(fdIn, 0);
			fclose(newInput);
		}
		dup2(listaPipes[0][1], 1);
		close(listaPipes[0][1]); // CUIDAO
		execvp(line->commands[0].filename, line->commands[0].argv);
		exit(1);
	}
	else{		
		for(int i = 1; i < (line->ncommands-1); i++) {
			pipe(listaPipes[i]);
			close(listaPipes[i-1][1]);
			int pidN = fork();
			if (pidN == 0) {
				dup2(listaPipes[i-1][0], 0);
				dup2(listaPipes[i][1], 1);
				close(listaPipes[i][1]);
				close(listaPipes[i-1][0]);
				close(listaPipes[i][1]);
				execvp(line->commands[i].filename, line->commands[i].argv);
				exit(1);
			}
			else{
				close(listaPipes[i-1][0]);
			}
		}
		close(listaPipes[line->ncommands-2][1]);
		int pid2 = fork();
		if (pid2 == 0) {
			dup2(listaPipes[line->ncommands-2][0], 0);
			close(listaPipes[line->ncommands-2][0]);
			if (boolOut) { //si hay red. de salida
				newOutput = fopen(line-> redirect_output, "w+"); //abrimos el fichero como escritura
				fdOut= fileno(newOutput);
				dup2(fdOut, 1);
				fclose(newOutput);
			}
			if (boolErr) { //si hay red. de salida
				newErr = fopen(line-> redirect_error, "w+"); //abrimos el fichero como escritura
				fdErr= fileno(newErr);
				dup2(fdErr, 2);
				fclose(newErr);
			}
			execvp(line->commands[line->ncommands-1].filename, line->commands[line->ncommands-1].argv);
			exit(1);
		}
		else {
			waitpid(pid2, NULL, 0);
			for (int i = 0; i < (line->ncommands-1); i++) {
				wait(NULL);
			}
		}
	}

}


