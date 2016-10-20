#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[])
{
  int p2cPipefd[2];
  int c2pPipefd[2];
  pid_t cpid;
  int buf;
	const int sz = 4096;
	char bufFile[sz];
	int zero = 0;
  const size_t pipeMsgSize = sizeof(int);
	FILE *inFile, *outFile;
	char confirm[5];

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
		printf("check c1 \n");
    while (read(p2cPipefd[0], &buf, pipeMsgSize) > 0){
			printf("check c2 \n");
			if(buf != 0){    /* read block # and size */
        cBlockNum = buf;
				read(p2cPipefd[0], &buf, pipeMsgSize);
				cBlockSz = buf;
        //TODO:read from the shared mem and write to the copy
        write(c2pPipefd[1], &cBlockNum, pipeMsgSize);
				printf("check c3 \n");
      }  else{    /* done. close out and msg 0 to parent */
				write(c2pPipefd[1], &zero, pipeMsgSize);
        //TODO: unlink shared mem
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
    do {
			//TODO: copy to shared mem
			fread(&bufFile, sz, 1, inFile);
			if (ferror(inFile)) {
				perror("error reading from input file");
				break;
			}
			if (feof(inFile)) {
				write(p2cPipefd[1], &zero, pipeMsgSize);
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
		fclose(inFile);
		fclose(outFile);
    exit(EXIT_SUCCESS);
  }
}