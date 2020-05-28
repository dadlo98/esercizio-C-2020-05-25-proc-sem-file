#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#define FILE_SIZE (1024*1024)
#define N 4
#define CHECK_ERR(a,msg) {if ((a) == -1) { perror((msg)); exit(EXIT_FAILURE); } }
#define CHECK_ERR_MMAP(a,msg) {if ((a) == MAP_FAILED) { perror((msg)); exit(EXIT_FAILURE); } }

sem_t * proc_sem;
sem_t * mutex;

void soluzioneA();
void soluzioneB();

int main(int argc, char * argv[]){
	int res;

	proc_sem = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			sizeof(sem_t) * 2, // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1, 0); // offset nel file
	CHECK_ERR_MMAP(proc_sem, "mmap")

	mutex = proc_sem + 1;

	res = sem_init(proc_sem, 1, 0);
	CHECK_ERR(res,"sem_init");

	res = sem_init(mutex, 1, 1);
	CHECK_ERR(res,"sem_init");

	printf("ora avvio la soluzione_A()...\n");
	soluzioneA();

	printf("ed ora avvio la soluzione_B()...\n");
	soluzioneB();

	res = sem_destroy(proc_sem);
	CHECK_ERR(res,"sem_destroy");
	res = sem_destroy(mutex);
	CHECK_ERR(res,"sem_destroy");

	return 0;
}

void soluzioneA(){
	int fd;
	int res;

	fd = open("outputA.txt", O_CREAT | O_TRUNC | O_RDWR,  S_IRUSR | S_IWUSR);
	CHECK_ERR(fd,"open");

	res = ftruncate(fd, FILE_SIZE);
	CHECK_ERR(res,"ftruncate");

	char enter;
	char ch;

	for(int i=0; i<N; i++){

		enter = 'A' + i;

		switch(fork()){
			case -1:
				perror("fork");
				exit(EXIT_FAILURE);

			case 0:
				while(1){

					res = sem_wait(proc_sem);
					CHECK_ERR(res,"sem_wait");

					res = sem_wait(mutex);
					CHECK_ERR(res,"sem_wait");
					
					//setto file offset dove il file è = 0
					for(int i=0; i<FILE_SIZE; i++){
						res = read(fd, &ch, 1);
						CHECK_ERR(res,"read");
						if(ch == 0)
							break;
					}

					//non ho trovato posizioni libere;
					if(lseek(fd, 0, SEEK_CUR) == FILE_SIZE) {
						res = sem_post(mutex);
						CHECK_ERR(res,"sem_post");

						exit(EXIT_SUCCESS);
					}	

					//setto fd di uno indietro perché read ha portato 
					//avanti di uno rispetto la posizione interessata
					res = lseek(fd, -1, SEEK_CUR);
					CHECK_ERR(res,"lseek");

					res = write(fd, &enter, sizeof(enter));
					CHECK_ERR(res,"write");

					res = sem_post(mutex);
					CHECK_ERR(res,"sem_post");
				}

				close(fd);
				exit(EXIT_SUCCESS);

				break;

			default:
				;	
		}
	}

	for(int i=0; i<FILE_SIZE + N; i++){
		res = sem_post(proc_sem);
		CHECK_ERR(res,"sem_post");
	}

	for(int i=0; i<N; i++){
		res = wait(NULL);
		CHECK_ERR(res,"wait");
	}
}

void soluzioneB() {
	int fd;
	int res;
	char * addr;

	fd = open("outputB.txt", O_CREAT | O_TRUNC | O_RDWR,  S_IRUSR | S_IWUSR);
	CHECK_ERR(fd,"open");

	res = ftruncate(fd, FILE_SIZE);
	CHECK_ERR(res,"ftruncate");

	addr = mmap(NULL, FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	CHECK_ERR_MMAP(addr,"mmap");

	close(fd);

	char enter;
	int check;

	for(int i=0; i<N; i++){

		enter = 'A' + i;

		switch(fork()){
			case -1:
				perror("fork");
				exit(EXIT_FAILURE);

			case 0:
				while(1){

					res = sem_wait(proc_sem);
					CHECK_ERR(res,"sem_wait");

					res = sem_wait(mutex);
					CHECK_ERR(res,"sem_wait");

					check = 0;

					for(int i=0; i<FILE_SIZE; i++) {
						if(addr[i] != 0)
						check++;

						if(addr[i] == 0)
						break;
					}

					if(check == FILE_SIZE) {
						res = sem_post(mutex);
						CHECK_ERR(res,"sem_post");

						exit(EXIT_SUCCESS);
					}
					else {
						addr[check] = enter;
						check++;
					}

					res = sem_post(mutex);
					CHECK_ERR(res,"sem_post");
				}

				exit(EXIT_SUCCESS);

				break;

			default:
				;	
		}
	}

	for(int i=0; i<FILE_SIZE + N; i++){
		res = sem_post(proc_sem);
		CHECK_ERR(res,"sem_post");
	}

	for(int i=0; i<N; i++){
		res = wait(NULL);
		CHECK_ERR(res,"wait");
	}
}
