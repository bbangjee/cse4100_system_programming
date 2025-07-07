#include "csapp.h"
#include <time.h>
#include <sys/time.h>
#define MAX_CLIENT 50 // 만들 수 있는 최대 client 개수
#define ORDER_PER_CLIENT 10 // client 하나당 server로 보내는 request의 개수
#define STOCK_NUM 10 // stock file에 있는 주식 항목의 최대 개수
#define BUY_SELL_MAX 10 // 주식을 사고 팔 때 한 request당 최대 항목의 개수 적절히 설정하여 사용 가능

int main(int argc, char **argv)
{
  // struct timeval start, end;
  // double elapsed_time;

  // 측정 시작
  // gettimeofday(&start, NULL);

  pid_t pids[MAX_CLIENT];
  int runprocess = 0, status, i;

  int clientfd, num_client;
  char *host, *port, buf[MAXLINE], tmp[3];
  rio_t rio;

  if (argc != 4)
  {
    fprintf(stderr, "usage: %s <host> <port> <client#>\n", argv[0]);
    exit(0);
  }

  host = argv[1];
  port = argv[2];
  num_client = atoi(argv[3]);

  /*	fork for each client process	*/
  while (runprocess < num_client)
  {
    // wait(&state);
    pids[runprocess] = fork();

    if (pids[runprocess] < 0)
      return -1;
    /*	child process		*/
    else if (pids[runprocess] == 0)
    {
      printf("child %ld\n", (long)getpid());

      clientfd = Open_clientfd(host, port);
      Rio_readinitb(&rio, clientfd);
      srand((unsigned int)getpid());

      for (i = 0; i < ORDER_PER_CLIENT; i++)
      {
        int option = rand() % 3;
        if (option == 0)
        { 
          // show
          strcpy(buf, "show\n");
        }

        else if (option == 1)
        {
          // buy
          int list_num = rand() % STOCK_NUM + 1;
          int num_to_buy = rand() % BUY_SELL_MAX + 1; // 1~10

          strcpy(buf, "buy ");
          sprintf(tmp, "%d", list_num);
          strcat(buf, tmp);
          strcat(buf, " ");
          sprintf(tmp, "%d", num_to_buy);
          strcat(buf, tmp);
          strcat(buf, "\n");
        }
        else if (option == 2)
        { 
          // sell
          int list_num = rand() % STOCK_NUM + 1;
          int num_to_sell = rand() % BUY_SELL_MAX + 1; // 1~10

          strcpy(buf, "sell ");
          sprintf(tmp, "%d", list_num);
          strcat(buf, tmp);
          strcat(buf, " ");
          sprintf(tmp, "%d", num_to_sell);
          strcat(buf, tmp);
          strcat(buf, "\n");
        }
        
        printf("Client %ld sending: %s", (long)getpid(), buf);

        Rio_writen(clientfd, buf, strlen(buf));
        // Rio_readlineb(&rio, buf, MAXLINE);
        Rio_readnb(&rio, buf, MAXLINE);
        Fputs(buf, stdout);

        usleep(1000000);
      }

      Close(clientfd);
      exit(0);
    }
    runprocess++;
  }
  for (i = 0; i < num_client; i++)
  {
    waitpid(pids[i], &status, 0);
  }
  /*
  gettimeofday(&end, NULL);
  elapsed_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
  int total_requests = num_client * ORDER_PER_CLIENT;
  double throughput = total_requests / elapsed_time;
  
  printf("=== PERFORMANCE RESULT ===\n");
  printf("Total time: %.3f seconds\n", elapsed_time);
  printf("Total requests: %d\n", total_requests);
  printf("Throughput: %.2f requests/sec\n", throughput);
  */
  return 0;
}
