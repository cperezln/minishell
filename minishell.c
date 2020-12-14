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
	char command[1024];
	int number;
	} CommandBG;
//Funciones Ejecutar
void exec1Command(int boolIn, int boolOut, int boolErr, tline* line);
void execNCommand(int boolIn, int boolOut, int boolErr, tline* line);
void handler(int sig);
void handlerSI(int sig);
void handlerSQ(int sig);
void delete(int pid);
void metadelete();
void jobs();
//Variables globales
FILE * newInput, *newOutput, *newErr; 
tline * line;
int status;
int fdIn, fdOut, fdErr;
char directorioActual[512];
int finishBg = 0;
int commandBgNumber = 0;
int * pidFCommands;
int * fgCommands;
CommandBG * bgList;
int nFCommands = 1;
int lastNumber = 0;
int bg, boolean;
int main(void) {
	bgList = (CommandBG*)malloc(sizeof(CommandBG));
	pidFCommands = (int *) malloc(sizeof(int));
	char buf[1024];
	char *dir;
	int boolIn, boolOut, boolErr;
	printf("%s==> ",getcwd(directorioActual, 512));		
	signal(SIGUSR1, handler);
	signal(SIGINT, handlerSI);
	signal(SIGQUIT, handlerSQ);
	boolean = 0;
	while (fgets(buf, 1024, stdin)) {
		boolean = 1;
		bg = 0;
		boolIn = 0;
		boolOut = 0;
		boolErr = 0;
		line = tokenize(buf);
		fgCommands = (int*) malloc(sizeof(int)*(line->ncommands));
		if (line==NULL || !strcmp(buf, "\n")) {
			if (finishBg) {
				metadelete();
			}
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
		if(!strcmp("fg", line->commands[0].argv[0]) && line->ncommands == 1){ // Entramos si es un fg
			int nFg = atoi(line->commands[0].argv[1]);
			int i = 0;
			while(bgList[i].number != nFg && i < commandBgNumber) {
				i++;
			}
			if (bgList[i].number == nFg){
				waitpid(bgList[i].pid, NULL, 0);
			}
		}
		if (line->redirect_input != NULL) {
			newInput = fopen(line-> redirect_input, "r"); //abrimos el fichero como escritura
			fdIn = fileno(newInput);			
			boolIn = 1; //declaramos una variable booleana para distinguir casos a continuación
		}
		if (line->redirect_output != NULL) {
			newOutput = fopen(line-> redirect_output, "w+"); //abrimos el fichero como escritura
			fdOut= fileno(newOutput);
			boolOut = 1;
		}
		if (line->redirect_error != NULL) {
			newErr = fopen(line-> redirect_error, "w+"); //abrimos el fichero como escritura
			fdErr= fileno(newErr);
			boolErr = 1;
		}
		if (line->background) {
			bg = 1;
		} 
		if (line->ncommands == 1 && strcmp("jobs", line->commands[0].argv[0])) { // Diferenciar caso un único mandato
			if (bg) {
				int pidBg = fork();
				if (pidBg == 0) {
					signal(SIGINT, SIG_IGN);
					signal(SIGQUIT, SIG_IGN);
					exec1Command(boolIn, boolOut, boolErr, line);
					kill(getppid(), SIGUSR1); 
					exit(0);
				}
				else{
					(bgList + commandBgNumber)-> pid = pidBg;
					strcpy((bgList + commandBgNumber)-> command, buf);
					lastNumber++;
					(bgList + commandBgNumber)-> number = lastNumber;
					commandBgNumber++;					
					bgList = (CommandBG*) realloc(bgList, sizeof(CommandBG)*(commandBgNumber+1));
					fprintf(stderr, "[%d]  %d\n", commandBgNumber, pidBg);
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
					signal(SIGINT, SIG_IGN);
					signal(SIGQUIT, SIG_IGN);
					execNCommand(boolIn, boolOut, boolErr, line);
					kill(getppid(), SIGUSR1); 
					exit(0);
				}
				else{
					(bgList + commandBgNumber)-> pid = pidBg;
					strcpy((bgList + commandBgNumber)-> command, buf);
					lastNumber++;
					(bgList + commandBgNumber)-> number = lastNumber ;
					commandBgNumber++;					
					bgList = (CommandBG*) realloc(bgList, sizeof(CommandBG)*(commandBgNumber + 1));
					fprintf(stderr, "[%d]  %d\n", commandBgNumber, pidBg);
				}
			}
			else{
				execNCommand(boolIn, boolOut, boolErr, line);
			}		
		}
		if(!strcmp("jobs", line->commands[0].argv[0]) && line->ncommands == 1){ // Entramos si es un jobs
			jobs();
		}
		if (finishBg) {
			metadelete();
		}
		if (boolean) {
			fprintf(stderr, "%s==> ",getcwd(directorioActual,512));
		}
		free(fgCommands);
	}
	return 0;
}

void exec1Command(int boolIn, int boolOut, int boolErr, tline* line){
	int pid = fork();
	if (pid == 0) {
		if (boolIn) { //si hay red. de entrada
			dup2(fdIn, 0);
			fclose(newInput);
		}
		if (boolOut) { //si hay red. de salida
			dup2(fdOut, 1);
			fclose(newOutput);
		}
		if (boolErr) { //si hay red. de salida
			dup2(fdErr, 2);
			fclose(newErr);
		}
		execvp(line->commands[0].filename, line->commands[0].argv);
		exit(1);
	}
	else{
		fgCommands[0] = pid;	
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
			dup2(fdIn, 0);
			fclose(newInput);
		}
		dup2(listaPipes[0][1], 1);
		close(listaPipes[0][1]); // CUIDAO
		execvp(line->commands[0].filename, line->commands[0].argv);
		exit(1);
	}
	else{
		fgCommands[0] = pid1;		
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
				fgCommands[i] = pidN;
				close(listaPipes[i-1][0]);
			}
		}
		close(listaPipes[line->ncommands-2][1]);
		int pid2 = fork();
		if (pid2 == 0) {
			dup2(listaPipes[line->ncommands-2][0], 0);
			close(listaPipes[line->ncommands-2][0]);
			if (boolOut) { //si hay red. de salida
				dup2(fdOut, 1);
				fclose(newOutput);
			}
			if (boolErr) { //si hay red. de salida
				dup2(fdErr, 2);
				fclose(newErr);
			}
			if(strcmp("jobs", line->commands[line->ncommands-1].argv[0])) {
				execvp(line->commands[line->ncommands-1].filename, line->commands[line->ncommands-1].argv);
			}
			else{
				jobs();
			}
			exit(1);
		}
		else {
			fgCommands[line->ncommands-1] = pid2;
			close(listaPipes[line->ncommands-2][0]);
			for (int i = 0; i < (line->ncommands); i++) {
				wait(NULL);
			}
		}
	}
}
void handler(int sig){
	int pidf = wait(NULL);
	*(pidFCommands + nFCommands) = pidf;
	nFCommands++;
	pidFCommands = (int *) realloc(pidFCommands, sizeof(int)*(nFCommands));
	finishBg = 1;
}
void delete(int pid){
	int bool = 0;
	int j;
	CommandBG *aux = (CommandBG*) malloc(sizeof(CommandBG)*(commandBgNumber));
	for (int i = 0; i < commandBgNumber; i++){
		if ((pid != bgList[i].pid) && (!bool)) {
			aux[i].pid = bgList[i].pid;
			aux[i].number = bgList[i].number;
			strcpy(aux[i].command, bgList[i].command);			
		}
		else if(pid == bgList[i].pid){
			bool = 1;
			j = i;
		}
		else{
			aux[i-1].pid = bgList[i].pid;
			strcpy(aux[i-1].command, bgList[i].command);
			aux[i-1].number = bgList[i].number;
		}
	}
	char auxStr[1024];
	strcpy(auxStr, bgList[j].command);
	strtok(auxStr, "&");
	strcat(auxStr, "\n");
	fprintf(stderr, "[%d] Hecho\t\t\t%s", bgList[j].number, auxStr);
	commandBgNumber--;
	bgList = (CommandBG*) realloc(bgList, sizeof(CommandBG)*(commandBgNumber));
	bgList = aux;
}
void metadelete(){
	finishBg = 0;
	for (int i = 1; i < nFCommands; i++){
		delete(pidFCommands[i]);			
	}
	free(pidFCommands);
	nFCommands = 1;
	pidFCommands = (int*) malloc(sizeof(int));
	lastNumber = bgList[commandBgNumber-1].number;
}
void jobs(){
	metadelete();
	for (int i = 0; i < commandBgNumber; i++) {
		fprintf(stderr, "[%d] Ejecutando \t\t\t%s", bgList[i].number, bgList[i].command);
	}
}
void handlerSI(int sig){
	if(bg != 0 && boolean){
		for (int i = 0; i < line->ncommands; i++){
			waitpid(fgCommands[i], NULL, 0);
		}
	}
	else{
		fprintf(stderr, "\n%s==> ",getcwd(directorioActual,512));
	}
	boolean = 0;
}
void handlerSQ(int sig){
	if(bg != 0 && boolean){
	if(bg != 0 && boolean){
		for (int i = 0; i < line->ncommands; i++){
			waitpid(fgCommands[i], NULL, 0);
		}
	}
	else{
		fprintf(stderr, "\n%s==> ",getcwd(directorioActual,512));
	}
	boolean = 0;
	}
}
