#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>

int main(int argc, char *argv[])
{
  int p2cPipefd[2];
  int c2pPipefd[2];
  pid_t cpid;
  int buf;
	const int sz = 4096;
	int zero = 0;
  const size_t pipeMsgSize = sizeof(int);
	FILE *inFile, *outFile;
	char confirm[5];
	char test[]="Please Baby Jesus Help ME!";
	const char *name = "/gcd15b";	// shared file name
	int shm_fd;		// file descriptor, from shm_open()
  char *shm_base;	// base address, from mmap()

  if (argc != 3) {
    fprintf(stderr, "Usage: trans input-file output-file\n");
    exit(EXIT_FAILURE);
  }
  if ((inFile = fopen(argv[1], "rb")) == NULL) {
		perror("can't open input file");
		exit(EXIT_FAILURE);
	}
	if ((outFile = fopen(argv[2], "r")) != NULL) {
		printf("***WARNING*** FILE %s already exists \n", argv[2]);
		printf("Do you wish to overwrite? Enter \"yes\" to overwrite or anything else to abort. \n");
		fgets(confirm, 4, stdin);
		if (strcmp(confirm, "yes") == 0) {
			if ((outFile = freopen(NULL, "wb", outFile)) == NULL) {
				perror("can't open output file");
				exit(EXIT_FAILURE);
			}
		} else {
			printf("Operation Canceled \n");
			exit(EXIT_SUCCESS);
		}
	} else {
		if ((outFile = fopen(argv[2], "w")) == NULL) {
			perror("can't open output file");
			exit(EXIT_FAILURE);
		}
	}

  if (pipe(p2cPipefd) == -1) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }
  if (pipe(c2pPipefd) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}

  cpid = fork();
  if (cpid == -1) {
    perror("fork");
    exit(EXIT_FAILURE);
  }
  if (cpid == 0) {    /* Child process */
		int cBlockNum=1, cBlockSz=0;
		close(p2cPipefd[1]);          /* Close unused write end */
		close(c2pPipefd[0]);          /* Close unused read end */
    //TODO: open shared mem
		shm_fd = shm_open(name, O_RDONLY, 0666);
		if (shm_fd == -1) {
			printf("cons: Shared memory failed: %s\n", strerror(errno));
			_exit(EXIT_FAILURE);
		}
	  /* map the shared memory segment to the address space of the process */
		shm_base = mmap(0, sz, PROT_READ, MAP_SHARED, shm_fd, 0);
		if (shm_base == MAP_FAILED) {
			printf("cons: Map failed: %s\n", strerror(errno));
			/* close the shared memory segment as if it was a file */
			if (close(shm_fd) == -1) {
				printf("cons: Close failed: %s\n", strerror(errno));
				_exit(EXIT_FAILURE);
			}
		  /* remove the shared memory segment from the file system */
			if (shm_unlink(name) == -1) {
				printf("cons: Error removing %s: %s\n", name, strerror(errno));
				_exit(EXIT_FAILURE);
			}
			_exit(EXIT_FAILURE);
		}
		printf("check c1 \n");
    while (read(p2cPipefd[0], &buf, pipeMsgSize) > 0){
			printf("check c2 \n");
			if(buf != 0){    /* read block # and size */
        cBlockNum = buf;
				read(p2cPipefd[0], &buf, pipeMsgSize);
				cBlockSz = buf;
				printf("told to read %d bytes\n", cBlockSz);
        //TODO:read from the shared mem and write to the copy
				printf("child reads\n");
				write(STDOUT_FILENO, shm_base, cBlockSz);
				printf("\n");
				int out = fwrite(shm_base, cBlockSz, 1, outFile);
				fflush(outFile);
				printf("wrote out %d blocks\n", out);
				if(ferror(outFile)) {
					perror("error writing to output file");
					write(c2pPipefd[1], &zero, pipeMsgSize);
					close(p2cPipefd[0]);
					close(c2pPipefd[1]);
					_exit(EXIT_FAILURE);
				}
        write(c2pPipefd[1], &cBlockNum, pipeMsgSize);
				printf("check c3 \n");
      }  else{    /* done. close out and msg 0 to parent */
				write(c2pPipefd[1], &zero, pipeMsgSize);
        //TODO: unlink shared mem
				/* remove the mapped shared memory segment from the address space of the process */
				if (munmap(shm_base, sz) == -1) {
					printf("cons: Unmap failed: %s\n", strerror(errno));
					_exit(EXIT_FAILURE);
				}
				/* close the shared memory segment as if it was a file */
				if (close(shm_fd) == -1) {
					printf("cons: Close failed: %s\n", strerror(errno));
					_exit(EXIT_FAILURE);
				}
			  /* remove the shared memory segment from the file system */
				if (shm_unlink(name) == -1) {
					printf("cons: Error removing %s: %s\n", name, strerror(errno));
					_exit(EXIT_FAILURE);
				}
      }
    }
    close(p2cPipefd[0]);
		close(c2pPipefd[1]);
    _exit(EXIT_SUCCESS);

  } else {            /* Parent process */
		printf("check p1 \n");
		int pBlockNum=1, pBlockSz=0;
		close(p2cPipefd[0]);          /* Close unused read end */
		close(c2pPipefd[1]);          /* Close unused write end */
    //TODO: create shared mem
		/* create the shared memory segment as if it was a file */
		shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
		if (shm_fd == -1) {
			printf("prod: Shared memory failed: %s\n", strerror(errno));
			_exit(EXIT_FAILURE);
		}
	  /* configure the size of the shared memory segment */
		ftruncate(shm_fd, sz);
		/* map the shared memory segment to the address space of the process */
		shm_base = mmap(0, sz, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
		if (shm_base == MAP_FAILED) {
			printf("prod: Map failed: %s\n", strerror(errno));
			// close and shm_unlink?
			_exit(EXIT_FAILURE);
		}
    do {
			//TODO: copy to shared mem
			pBlockSz = fread(shm_base, 1, sz, inFile);
			printf("%d bytes read from the inFile\n", pBlockSz);
			if (ferror(inFile)) {
				perror("error reading from input file");
				break;
			}
			if (pBlockSz == 0) {
				write(p2cPipefd[1], &zero, pipeMsgSize);
				printf("hit the eof\n");
			} else {
				write(p2cPipefd[1], &pBlockNum, pipeMsgSize);
				write(p2cPipefd[1], &pBlockSz, pipeMsgSize);
				++pBlockNum;
			}
			printf("check p2 \n");
			read(c2pPipefd[0], &buf, pipeMsgSize);
		} while (buf > 0);

    close(p2cPipefd[1]);          /* Reader will see EOF */
		close(c2pPipefd[0]);
		printf("check p3 \n");
    wait(NULL);                /* Wait for child */
		printf("check p4 \n");
		/* remove the mapped memory segment from the address space of the process */
	  if (munmap(shm_base, sz) == -1) {
			printf("prod: Unmap failed: %s\n", strerror(errno));
			_exit(EXIT_FAILURE);
		}
	  /* close the shared memory segment as if it was a file */
		if (close(shm_fd) == -1) {
			printf("prod: Close failed: %s\n", strerror(errno));
			_exit(EXIT_FAILURE);
		}
		fclose(inFile);
		fclose(outFile);
    exit(EXIT_SUCCESS);
  }
}