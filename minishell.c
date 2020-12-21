/* Minishell desarrollada por alumnos de la URJC del Doble Grado de Ingeniería Informática y Matemáticas
 Juan Antonio Gordillo Gayo y Cristian Pếrez Corral
 para la asignatura de Sistemas Operativos
 el uso de dicho programa sólo se permitirá con fines académicos
 
 Finalizada  el día 19 de diciembre del 2020
 */

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
char buf[1024];
int finishBg = 0;
int commandBgNumber = 0;
int * pidFCommands;
int * fgCommands;
CommandBG * bgList;
int nFCommands = 1;
int lastNumber = 0;
int bg, boolean;
int auxiliarPid;
int main(void) {
	bgList = (CommandBG*)malloc(sizeof(CommandBG));
	pidFCommands = (int *) malloc(sizeof(int));
	int boolIn, boolOut, boolErr;
	printf("%s==> ",getcwd(directorioActual, 512));		
	signal(SIGUSR1, handler);	
	signal(SIGINT, handlerSI);
	signal(SIGQUIT, handlerSQ);
	signal(SIGUSR2, handler);
	boolean = 0;
	while (fgets(buf, 1024, stdin)) {
		auxiliarPid = -1;
		boolean = 1;
		bg = 0;
		boolIn = 0;
		boolOut = 0;
		boolErr = 0;
		line = tokenize(buf);
		fgCommands = (int*) malloc(sizeof(int)*(line->ncommands));
		if (line==NULL || !strcmp(buf, "\n")) { //Si no introducen nada entramos aquí
			if (finishBg) {
				metadelete();
			}
			printf("%s==> ",getcwd(directorioActual, 512));	
			continue;
		}
		if(!strcmp("cd", line->commands[0].argv[0]) && line->ncommands == 1){ // Entramos si es un cd
			char * dir;
			if (line->commands[0].argc > 2) {
				fprintf(stderr, "Demasiados argumentos para cd. Se espera 1, pero %d recibidos\n", line->commands[0].argc);
			}
			else {
				if (line->commands[0].argc == 1) {
					dir = getenv("HOME");
				}
				else{
					dir = line->commands[0].argv[1];
				}
				if (chdir(dir) != 0) {
					fprintf(stderr, "Error al cambiar de directorio %s.\n", strerror(errno));
				}
			}
		}
		if(!strcmp("fg", line->commands[0].argv[0]) && line->ncommands == 1){ // Entramos si es un fg
			int i = 0;
			if (line->commands[0].argc == 2){
				int nFg = atoi(line->commands[0].argv[1]);
				while(bgList[i].number != nFg && i < commandBgNumber) { //buscamos el proceso bg que queremos pasar a fg
					i++;
				}
				if (bgList[i].number == nFg){
					auxiliarPid = bgList[i].pid;
					kill(auxiliarPid, SIGUSR2);
					fprintf(stderr, "%s\n", bgList[i].command);
					waitpid(auxiliarPid, NULL, 0); //esperamos ese proceso (pasarlo a fg)
					delete(auxiliarPid);
					nFCommands--;
					fgCommands[0] = auxiliarPid;
					finishBg = 1;
				}
				else {
					fprintf(stderr, "Proceso no encontrado en background\n");
				}
			}
			else{
				fprintf(stderr, "Número de parámetros incorrecto para fg. Se espera 1, fueron %d introducidos\n", line->commands[0].argc - 1);
			}
		}
		if (line->redirect_input != NULL) {
			newInput = fopen(line-> redirect_input, "r"); //abrimos el fichero como escritura
			if (newInput != NULL) {
				fdIn = fileno(newInput);			
				boolIn = 1; //declaramos una variable booleana para distinguir casos a continuación
			}
			else {
				fprintf(stderr, "Ha habido un error al abrir el fichero. %s\n", strerror(errno));
				fprintf(stderr, "%s==> ",getcwd(directorioActual,512));
				continue;
			}
		}
		if (line->redirect_output != NULL) {
			newOutput = fopen(line-> redirect_output, "w+"); //abrimos el fichero como escritura
			if (newOutput != NULL) {
				fdOut= fileno(newOutput);
				boolOut = 1; 
			}
			else {
				fprintf(stderr, "Ha habido un error al abrir el fichero. %s\n", strerror(errno));
				fprintf(stderr, "%s==> ",getcwd(directorioActual,512));
				continue;
			}
		}
		if (line->redirect_error != NULL) {
			newErr = fopen(line-> redirect_error, "w+"); //abrimos el fichero como escritura
			if (newErr != NULL) {
				fdErr= fileno(newErr);
				boolErr = 1;
			}
			else {
				fprintf(stderr, "Ha habido un error al abrir el fichero. %s\n", strerror(errno));
				fprintf(stderr, "%s==> ",getcwd(directorioActual,512));
				continue;				
			}
		}
		if (line->background) {
			bg = 1;
		} 
		if (line->ncommands == 1 && strcmp("jobs", line->commands[0].argv[0]) && strcmp("fg", line->commands[0].argv[0]) && strcmp("cd", line->commands[0].argv[0])) { // Diferenciar caso un único mandato
			if (bg) {//diferenciamos el caso que sea background
				int pidBg = fork();
				if (pidBg == 0) {
					signal(SIGINT, SIG_IGN); //ignoramos las señales para que los procesos de bg no mueran con estas
					signal(SIGQUIT, SIG_IGN);
					exec1Command(boolIn, boolOut, boolErr, line);
					kill(getppid(), SIGUSR1); 
					exit(0);
				}
				else{
					(bgList + commandBgNumber)-> pid = pidBg; //el padre no espera; pero introduce el hijo en la lista de procesos de bg, y continua con su ejecución
					strcpy((bgList + commandBgNumber)-> command, buf);
					lastNumber++;
					(bgList + commandBgNumber)-> number = lastNumber;
					commandBgNumber++;					
					bgList = (CommandBG*) realloc(bgList, sizeof(CommandBG)*(commandBgNumber+1));
					fprintf(stderr, "[%d]  %d\n", commandBgNumber, pidBg);
				}
			}
			else{ //caso que no sea bg
				exec1Command(boolIn, boolOut, boolErr, line);
			}
		}
		else if (line->ncommands>1) { //Diferenciar caso múltiples mandatos
			if (bg) {//diferenciamos el caso que sea background
				int pidBg = fork();
				if (pidBg == 0) {
					signal(SIGINT, SIG_IGN);//ignoramos las señales para que los procesos de bg no mueran con estas
					signal(SIGQUIT, SIG_IGN);
					execNCommand(boolIn, boolOut, boolErr, line);
					kill(getppid(), SIGUSR1); 
					exit(0);
				}
				else{
					(bgList + commandBgNumber)-> pid = pidBg;//el padre no espera; pero introduce el hijo en la lista de procesos de bg, y continua con su ejecución
					strcpy((bgList + commandBgNumber)-> command, buf);
					lastNumber++;
					(bgList + commandBgNumber)-> number = lastNumber ;
					commandBgNumber++;					
					bgList = (CommandBG*) realloc(bgList, sizeof(CommandBG)*(commandBgNumber + 1));
					fprintf(stderr, "[%d]  %d\n", commandBgNumber, pidBg);
				}
			}
			else{//caso que no sea bg
				execNCommand(boolIn, boolOut, boolErr, line);
			}		
		}
		if(!strcmp("jobs", line->commands[0].argv[0]) && line->ncommands == 1){ // Entramos si es un jobs
			jobs();
		}
		if (finishBg) { //caso en el que hayan acabado procesos bg
			metadelete();
		}
		if (boolean) { //condicional auxiliar para no imprimir dos veces el prompt en caso de que no se haya hecho \n
			fprintf(stderr, "%s==> ",getcwd(directorioActual,512));
		}
		free(fgCommands);
		line = NULL;
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
		if (line->commands[0].filename == NULL) {
			fprintf(stderr, "Orden <<%s>> no encontrada.\n", line->commands[0].argv[0]);
		} 
		else {
			execvp(line->commands[0].filename, line->commands[0].argv); //Ejecutamos el mandato en el hijo
			exit(1);
		}
	}
	else{
		fgCommands[0] = pid;	//El padre espera
		wait(&status);
	}

}
void execNCommand(int boolIn, int boolOut, int boolErr, tline* line){
	int listaPipes[line->ncommands-1][2];
	pipe(listaPipes[0]);
	int pid1 = fork(); //primer hijo
	if (pid1 == 0) { //Creamos un primer hijo para el primer mandato del pipe
		close(listaPipes[0][0]);
		if (boolIn) { //si hay red. de entrada
			dup2(fdIn, 0);
			fclose(newInput);
		}
		dup2(listaPipes[0][1], 1);
		close(listaPipes[0][1]); 
		if (line->commands[0].filename == NULL) {
			fprintf(stderr, "Orden <<%s>> no encontrada.\n", line->commands[0].argv[0]);
		} 
		else {
			execvp(line->commands[0].filename, line->commands[0].argv);
			exit(1);
		}
	}
	else{
		fgCommands[0] = pid1;		
		for(int i = 1; i < (line->ncommands-1); i++) {//Vamos creando hijos sucesivamente para ir ejecutando los mandatos intermedios
			pipe(listaPipes[i]);
			close(listaPipes[i-1][1]);
			int pidN = fork();
			if (pidN == 0) {
				dup2(listaPipes[i-1][0], 0);
				dup2(listaPipes[i][1], 1);
				close(listaPipes[i][1]);
				close(listaPipes[i-1][0]);
				close(listaPipes[i][1]);
				if (line->commands[i].filename == NULL) {
					fprintf(stderr, "Orden <<%s>> no encontrada.\n", line->commands[i].argv[0]);
				} 
				else {
					execvp(line->commands[i].filename, line->commands[i].argv);
					exit(1);
				}
			}
			else{
				fgCommands[i] = pidN;
				close(listaPipes[i-1][0]);
			}
		}
		close(listaPipes[line->ncommands-2][1]);
		int pid2 = fork();
		if (pid2 == 0) { //El último hijo, que ejecuta el último mandato del pipe
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
				if (line->commands[line->ncommands-1].filename == NULL) {
					fprintf(stderr, "Orden <<%s>> no encontrada.\n", line->commands[line->ncommands-1].argv[0]);
				}
				else { 
					execvp(line->commands[line->ncommands-1].filename, line->commands[line->ncommands-1].argv);
					exit(1);
				}
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
	if(sig == SIGUSR1) {
		int pidf = wait(NULL); //el padre recoge el PID de el hijo que ha acabado de background
		*(pidFCommands + nFCommands) = pidf;
		nFCommands++;
		pidFCommands = (int *) realloc(pidFCommands, sizeof(int)*(nFCommands)); //Metemos en la lista de mandatos acabados en BG
		finishBg = 1;
	}
	else {
		signal(SIGINT, SIG_DFL);
	}
}
void delete(int pid){ //La función delete se encarga de borrar un mandato en concreto de la lista de mandatos en background, el que haya finalizado
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
	if (bgList[j].pid != auxiliarPid){
		fprintf(stderr, "[%d] Hecho\t\t\t%s", bgList[j].number, auxStr);
	}
	commandBgNumber--;
	bgList = (CommandBG*) realloc(bgList, sizeof(CommandBG)*(commandBgNumber));
	bgList = aux;
}
void metadelete(){ //Metadelete ejecuta el delete n veces, donde n es el número de mandatos en background acabados en esta iteración del while
	finishBg = 0;
	for (int i = 1; i < nFCommands; i++){
		delete(pidFCommands[i]);			
	}
	free(pidFCommands);
	nFCommands = 1;
	pidFCommands = (int*) malloc(sizeof(int));
	lastNumber = bgList[commandBgNumber-1].number;
}
void jobs(){ //Recorre la lista de procesos en bg y la muestra por pantalla
	metadelete();
	for (int i = 0; i < commandBgNumber; i++) {
		fprintf(stderr, "[%d] Ejecutando \t\t\t%s", bgList[i].number, bgList[i].command);
	}
}
void handlerSI(int sig){ //Handler para controlar las señales de Ctrl + C
	if(bg == 0 && line != NULL){
		for (int i = 0; i < line->ncommands; i++){
			kill(fgCommands[i], SIGINT);
			waitpid(fgCommands[i], NULL, 0);
		}
		
	}
	fprintf(stderr, "\n%s==> ",getcwd(directorioActual,512));
	boolean = 0;
}
void handlerSQ(int sig){//Handler para controlar las señales de Ctrl + Alt Gr 
	if(bg == 0 && line != NULL){
		for (int i = 0; i < line->ncommands; i++){
			kill(fgCommands[i], SIGINT);
			waitpid(fgCommands[i], NULL, 0);
		}
	}
	fprintf(stderr, "\n%s==> ",getcwd(directorioActual,512));
	boolean = 0;
}

