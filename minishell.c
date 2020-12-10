#include <stdio.h>
#include <unistd.h>
#include "parser.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
//Tipos 
typedef struct TCommandBG  {
	int pid;
	char * command;
	int number;
	} CommandBG;
//Funciones Ejecutar
void exec1Command(int boolIn, int boolOut, int boolErr, tline* line);
void execNCommand(int boolIn, int boolOut, int boolErr, tline* line);
void handler(int sig);
void delete(int pid,  CommandBG *bgList);
//Variables globales
FILE * newInput, *newOutput, *newErr; 
int status;
int fdIn, fdOut, fdErr;
char directorioActual[512];
int finishBg = 0;
int commandBgNumber = 0;
int * pidFCommands;
int nFCommands = 0;
int lastNumber =0;

int main(void) {
	CommandBG * bgList= (CommandBG*)malloc(sizeof(CommandBG));
	char buf[1024];
	char *dir;
	tline * line;
	int boolIn, boolOut, boolErr, bg;
	printf("%s==> ",getcwd(directorioActual, 512));		
	signal(SIGUSR1, handler);
	while (fgets(buf, 1024, stdin)) {
		bg = 0;
		boolIn = 0;
		boolOut = 0;
		boolErr = 0;
		line = tokenize(buf);
		if (line==NULL || !strcmp(buf, "\n")) {
			printf("%s==> ",getcwd(directorioActual, 512));	
			continue;
		}
		if(!strcmp("cd", line->commands[0].argv[0]) && line->ncommands == 1){ // Entramos si es un cd
			char * dir;
			if (line->commands[0].argc == 1) {
				dir = getenv("HOME");
			}
			else{
				dir = line->commands[0].argv[1];
			}
			chdir(dir);
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
			bg = 1;
		} 
		if (line->ncommands == 1) { // Diferenciar caso un único mandato
			if (bg) {
				int pidBg = fork();
				if (pidBg == 0) {
					exec1Command(boolIn, boolOut, boolErr, line);
					kill(getppid(), SIGUSR1); 
					exit(0);
				}
				else{
					(bgList + commandBgNumber)-> pid = pidBg;
					(bgList + commandBgNumber)-> command = buf;
					(bgList + commandBgNumber)-> number = lastNumber + 1;
					commandBgNumber++;					
					bgList = (CommandBG*) realloc(bgList, sizeof(CommandBG)*(commandBgNumber +1));
					fprintf(stderr, "[%d]+ : %d\n", commandBgNumber, pidBg);
				}
			}
			else{
				exec1Command(boolIn, boolOut, boolErr, line);
			}
		}
		else if (line->ncommands>1) { //Diferenciar caso múltiples mandatos
			if (bg) {
				int pidBg = fork();
				if (pidBg == 0) {
					execNCommand(boolIn, boolOut, boolErr, line);
					kill(getppid(), SIGUSR1); 
					exit(0);
				}
				else{
					(bgList + commandBgNumber)-> pid = pidBg;
					(bgList + commandBgNumber)-> command = buf;
					lastNumber++;
					(bgList + commandBgNumber)-> number = lastNumber ;
					commandBgNumber++;					
					bgList = (CommandBG*) realloc(bgList, sizeof(CommandBG)*(commandBgNumber +1));
					fprintf(stderr, "[%d]+ : %d\n", commandBgNumber, pidBg);
				}
			}
			else{
				execNCommand(boolIn, boolOut, boolErr, line);
			}		
		}
		
		if (finishBg) {
			finishBg = 0;
			for (int i = 1; i < nFCommands; i++){
				delete(pidFCommands[i], bgList);
				
			}
		}
		printf("%s==> ",getcwd(directorioActual,512));	
	}
	return 0;
}

void exec1Command(int boolIn, int boolOut, int boolErr, tline* line){
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
void execNCommand(int boolIn, int boolOut, int boolErr, tline* line){
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
			close(listaPipes[line->ncommands-2][0]);
			for (int i = 0; i < (line->ncommands); i++) {
				wait(NULL);
			}
		}
	}

}
void handler(int sig){
	int pidf = wait(NULL);
	nFCommands++;
	pidFCommands = (int *) realloc(pidFCommands, sizeof(int)*nFCommands);
	*(pidFCommands + nFCommands) = pidf;
	fprintf(stderr, "%d\n", *(pidFCommands + nFCommands));
	finishBg = 1;
}
void delete(int pid, CommandBG *bgList){
	int bool = 0;
	int j;
	CommandBG *aux = (CommandBG*) malloc(sizeof(CommandBG)*(commandBgNumber-1));
	for (int i = 0; i < commandBgNumber; i++){
		if ((pid != bgList[i].pid) && (!bool)) {
			aux[i].pid = bgList[i].pid;
			strcpy(aux[i].command, bgList[i].command);			
		}
		else if(pid == bgList[i].pid){
			bool = 1;
			j = i;
		}
		else{
			aux[i-1].pid = bgList[i].pid;
			strcpy(aux[i-1].command, bgList[i].command);
		}
	}
	fprintf(stderr, "[%d]:\t terminado\t%s",bgList[j].number,bgList[j].command);
	commandBgNumber--;
	free(bgList);
	bgList = aux;
	lastNumber = bgList[commandBgNumber].number+1;
}
