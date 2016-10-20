/* Grady C Denton
 * gcd15b
 * cop4610
 * project 3
 */
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
  int p2cPipefd[2];  //pipe file descriptors
  int c2pPipefd[2];
  pid_t cpid;  //process id
  int buf;  //for pipe messages
	const int sz = 4096;  //block size of shared mem
	int zero = 0;  //used to terminate exchange
  const size_t pipeMsgSize = sizeof(int);  //passing ints
	FILE *inFile, *outFile;  //input and output file
	char confirm[5];  //confirm overwrite
	const char *name = "/gcd15b";	// shared file name
	int shm_fd;		// file descriptor, from shm_open()
  char *shm_base;	// base address, from mmap()

	/*open the files and error check*/
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

  /*creat pipes*/
  if (pipe(p2cPipefd) == -1) {  //parent to child
    perror("pipe");
    exit(EXIT_FAILURE);
  }
  if (pipe(c2pPipefd) == -1) {  //child to parent
		perror("pipe");
		exit(EXIT_FAILURE);
	}

  cpid = fork();
  if (cpid == -1) {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  /* Child process */
  if (cpid == 0) {
		int cBlockNum=1, cBlockSz=0;
		close(p2cPipefd[1]);          /* Close unused write end */
		close(c2pPipefd[0]);          /* Close unused read end */
    //open shared mem
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
	    /* read block # and size */
	    while (read(p2cPipefd[0], &buf, pipeMsgSize) > 0){
			if(buf != 0){
        cBlockNum = buf;
				read(p2cPipefd[0], &buf, pipeMsgSize);
				cBlockSz = buf;
        //read from the shared mem and write to output file
				fwrite(shm_base, cBlockSz, 1, outFile);
				fflush(outFile);
				if(ferror(outFile)) {
					perror("error writing to output file");
					write(c2pPipefd[1], &zero, pipeMsgSize);
					close(p2cPipefd[0]);
					close(c2pPipefd[1]);
					_exit(EXIT_FAILURE);
				}
        write(c2pPipefd[1], &cBlockNum, pipeMsgSize);
      }  else{    /* done. close out and msg 0 to parent */
				write(c2pPipefd[1], &zero, pipeMsgSize);
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
    /*close pipes*/
    close(p2cPipefd[0]);
		close(c2pPipefd[1]);
    _exit(EXIT_SUCCESS);

  } else {            /* Parent process */
		int pBlockNum=1, pBlockSz=0;
		close(p2cPipefd[0]);          /* Close unused read end */
		close(c2pPipefd[1]);          /* Close unused write end */
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
			close(shm_fd);
			_exit(EXIT_FAILURE);
		}
    do {
			/*copy to shared mem*/
			pBlockSz = fread(shm_base, 1, sz, inFile); //read 1 byte at a time up to 4k times
			if (ferror(inFile)) {
				perror("error reading from input file");
				break;
			}
			if (pBlockSz == 0) {
				write(p2cPipefd[1], &zero, pipeMsgSize);
			} else {
				write(p2cPipefd[1], &pBlockNum, pipeMsgSize); //send block number to child
				write(p2cPipefd[1], &pBlockSz, pipeMsgSize); //send block size
				++pBlockNum;
			}
			/*wait for child acknowledgement*/
			read(c2pPipefd[0], &buf, pipeMsgSize);
		} while (buf > 0);

		/*cleanup*/
    close(p2cPipefd[1]);          /* Reader will see EOF */
		close(c2pPipefd[0]);
    wait(NULL);                /* Wait for child */
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