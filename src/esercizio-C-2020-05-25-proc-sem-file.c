#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

sem_t * proc_sem;
sem_t * mutex;

void soluzione_A() {    //only using open(), lseek(), write()
    int res;
    int fd;

    fd = open("outputA.txt", O_CREAT | O_TRUNC | O_RDWR,  S_IRUSR | S_IWUSR);
    if(fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    //zeroing the file

    res = ftruncate(fd, 0);
    if(res == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    res = ftruncate(fd, FILE_SIZE);
    if(res == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    //creating buffer to read file bytes

    char * buffer = malloc(FILE_SIZE);
    if (buffer == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }


    //need to use mmap to share sem and mutex

    proc_sem = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			2*sizeof(sem_t), // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1,
			0); // offset nel file

    res = sem_init(proc_sem, 1, 0);
    if(res == -1) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    mutex = proc_sem +1;

    res = sem_init(mutex, 1, 1);
    if(res == -1) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    //operations

    char enter;

    for(int i = 0; i < N; i++){
        switch(fork()) {
            case -1:
                perror("fork");
                exit(EXIT_FAILURE);

            case 0:
                enter = 'A' + (char) i;

                if(lseek(fd, 0, SEEK_SET) == -1) {
                    perror("lseek");
                    exit(EXIT_FAILURE);
                }

                res = read(fd, buffer, FILE_SIZE);
                    if(res == -1) {
                        perror("read");
                        exit(EXIT_FAILURE);
                }

				if(lseek(fd, 0, SEEK_SET) == -1) {
					perror("lseek");
					exit(EXIT_FAILURE);
				}

                if(sem_wait(proc_sem) == -1) {
                    perror("sem_wait");
                    exit(EXIT_FAILURE);
                }

                for(int j=0; j<FILE_SIZE; j++){
                    if(buffer[j] == 0) {
                        buffer[j] = enter;
                        if(sem_wait(mutex) == -1) {
                            perror("sem_wait");
                            exit(EXIT_FAILURE);
                        }

                        //critical seciton
                        write(fd, &enter, sizeof(enter));

                        if(sem_post(mutex) == -1) {
                            perror("sem_post");
                            exit(EXIT_FAILURE);
                        }
                    }
                }
                exit(0);

            default:
                for(int i=0; i < FILE_SIZE + N; i++) {
                    if(sem_post(proc_sem) == -1) {
                        perror("sem_post");
                        exit(EXIT_FAILURE);
                    }
                }

                if(wait(NULL) == -1){
                    perror("wait");
                    exit(EXIT_FAILURE);
                }
        }
    }
}

void soluzione_B() {   //only using open() and mmap()
    int res;
    int fd;
    char * addr;

    fd = open("outputB.txt", O_CREAT | O_TRUNC | O_RDWR,  S_IRUSR | S_IWUSR);
    if(fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    res = ftruncate(fd, FILE_SIZE);
    if(res == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    //creating shared mmap for file and sem+mutex

    addr = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			FILE_SIZE, // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED, // memory map condivisibile con altri processi
			fd,
			0); // offset nel file

    if(addr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    memset(addr, 0, FILE_SIZE);

    proc_sem = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			2*sizeof(sem_t), // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1,
			0); // offset nel file

    res = sem_init(proc_sem, 1, 0);
    if(res == -1) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    mutex = proc_sem +1;

    res = sem_init(mutex, 1, 1);
    if(res == -1) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    //operations

    char enter;

    for(int i = 0; i < N; i++){
        switch(fork()) {
            case -1:
                perror("fork");
                exit(EXIT_FAILURE);

            case 0:
                enter = 'A' + (char) i;

                if(sem_wait(proc_sem) == -1) {
                    perror("sem_wait");
                    exit(EXIT_FAILURE);
                }

                for(int j=0; j<FILE_SIZE; j++){
                    if(addr[j] == 0) {
                        if(sem_wait(mutex) == -1) {
                            perror("sem_wait");
                            exit(EXIT_FAILURE);
                        }

                        //critical seciton
                        write(fd, &enter, sizeof(enter));

                        if(sem_post(mutex) == -1) {
                            perror("sem_post");
                            exit(EXIT_FAILURE);
                        }
                    }
                }
                exit(0);

            default:
                for(int i=0; i < FILE_SIZE + N; i++) {
                    if(sem_post(proc_sem) == -1) {
                        perror("sem_post");
                        exit(EXIT_FAILURE);
                    }
                }

                if(wait(NULL) == -1){
                    perror("wait");
                    exit(EXIT_FAILURE);
                }
        }
    }
}

int main() {
    printf("ora avvio la soluzione_A()...\n");
    soluzione_A();

    printf("ed ora avvio la soluzione_B()...\n");
    soluzione_B();

    printf("bye!\n");
    return 0;
}
