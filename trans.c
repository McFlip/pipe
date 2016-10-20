#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[])
{
  int p2cPipefd[2];  //TODO: need 2 pipes
  int c2pPipefd[2];
  pid_t cpid;
  int buf;
  int test =33;
  const size_t pipeMsgSize = sizeof(int);
	FILE *inFile, *outFile;
	char confirm[5];

  if (argc != 3) {
    fprintf(stderr, "Usage: trans input-file output-file\n");
    exit(EXIT_FAILURE);
  }
  if ((inFile = fopen(argv[1], "r")) == NULL) {
		perror("can't open input file");
		exit(EXIT_FAILURE);
	}
	if ((outFile = fopen(argv[2], "r")) != NULL) {
		printf("***WARNING*** FILE %s already exists \n", argv[2]);
		printf("Do you wish to overwrite? Enter \"yes\" to overwrite or anything else to abort. \n");
		fgets(confirm, 4, stdin);
		if (strcmp(confirm, "yes") == 0) {
			if ((outFile = freopen(NULL, "w", outFile)) == NULL) {
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
  //TODO: another pipe

  cpid = fork();
  if (cpid == -1) {
    perror("fork");
    exit(EXIT_FAILURE);
  }
  //TODO: if parent fails & exit remember to kill child
  if (cpid == 0) {    /* Child process */
    close(p2cPipefd[1]);          /* Close unused write end */
		close(c2pPipefd[0]);          /* Close unused read end */
    //TODO: set up other pipe
    //TODO: open shared mem
		printf("check c1 \n");
    while (read(p2cPipefd[0], &buf, pipeMsgSize) > 0){
			printf("check c2 \n");
			if(buf != 0){    /* read block # and size */
        int cBlockNum = buf;
        int cBlockSz;
//         if (read(p2cPipefd[0], &buf, pipeMsgSize) > 0){
//           cBlockSz = buf;
//         }
        //TODO:read from the shared mem and write to the copy
        //TODO: msg cBlockNum back to ACK
        write(c2pPipefd[1], &test, pipeMsgSize);
				printf("check c3 \n");
      }  else{    /* done. close out and msg 0 to parent */
        //TODO: msg 0
        //TODO: unlink shared mem
      }
      write(STDOUT_FILENO, &buf, pipeMsgSize);
      printf("buf = %d \n", buf);
    }

    close(p2cPipefd[0]);  //TODO: close other pipe
		close(c2pPipefd[1]);
    _exit(EXIT_SUCCESS);

  } else {            /* Parent process */
		printf("check p1 \n");
		close(p2cPipefd[0]);          /* Close unused read end */
		close(c2pPipefd[1]);          /* Close unused write end */
    //TODO: other pipe
    //TODO: create shared mem
    //TODO: copy to shared mem
    write(p2cPipefd[1], &test, pipeMsgSize);
		printf("check p2 \n");
		if (read(c2pPipefd[0], &buf, pipeMsgSize) > 0){
			printf("parent recieve %d", buf);
		}
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

/* snynchronization
 * infinite loop
 * if you can read -- use poll()
 * if buf is zero
 * exit
 * else if buf is expected number
 * enter critical section
 * exit critical section - msg correct number
 * else if recieve -1 failboat
 * EXIT_FAILURE
 * end loop
 */
