#include "csapp.h"
#define DELIM_CHARS     " \n"

typedef struct stockNode
{
  int id;
  int left_stock;
  int price;
  int file_order;
  // int readcnt;
  // sem_t mutex;
  struct stockNode *left;
  struct stockNode *right;
} stockNode;

typedef struct
{
  int maxfd;                   /* read_set에서 가장 큰 descriptor */
  fd_set read_set;             /* Set of all active descriptors */
  fd_set ready_set;            /* Subset of ready descriptors ready for reading */
  int nready;                  /* select를 통한 ready descriptor 개수 */
  int maxi;                    /* High water index into client array */
  int clientfd[FD_SETSIZE];    /* set of active descriptors */
  rio_t clientrio[FD_SETSIZE]; /* set of active read buffers */
  // FD_SETSIZE = 1024
} pool;

int byte_cnt = 0;
stockNode *head = NULL;
int *original_order = NULL;
pool *global_pool = NULL;
int global_listenfd = -1;

void sigintHandler(int sig);
void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients(pool *p);
void scanFile();
int analyze_commands(char *buf, int connfd);
void buy_stock(int id, int amount, int connfd);
void show_stocks(int connfd);
void sell_stock(int id, int amount, int connfd);
void writeFile();
void check_and_update_file(pool *p);
void free_all_stocks(stockNode *root);
stockNode *insertStockNode(stockNode *root, int stockID, int left, int price, int order);
stockNode *search_stock(stockNode *root, int id);

int main(int argc, char **argv)
{
  Signal(SIGINT, sigintHandler);

  int listenfd, connfd;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  static pool pool;
  global_pool = &pool;

  char client_hostname[MAXLINE], client_port[MAXLINE];

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(0);
  }

  // stock.txt 읽어들이기
  scanFile();

  listenfd = Open_listenfd(argv[1]);
  global_listenfd = listenfd;

  init_pool(listenfd, &pool);

  while (1)
  {
    pool.ready_set = pool.read_set;
    pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);

    if (FD_ISSET(listenfd, &pool.ready_set))
    {
      clientlen = sizeof(struct sockaddr_storage);
      connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
      Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
      printf("Connected to (%s, %s)\n", client_hostname, client_port);
      add_client(connfd, &pool);
    }
    check_clients(&pool);
  }

  writeFile();
  free_all_stocks(head);
  free(original_order);
  exit(0);
}

void writeFile()
{
  FILE *stockfp = fopen("stock.txt", "w");
  if (stockfp == NULL)
  {
    perror("stock.txt write failure");
    return;
  }

  int i = 0;
  while (original_order[i] != 0)
  {
    stockNode *target = search_stock(head, original_order[i++]);
    if (target != NULL)
      fprintf(stockfp, "%d %d %d\n", target->id, target->left_stock, target->price);
  }

  fclose(stockfp);
}

void free_all_stocks(stockNode *root)
{
  if (root == NULL) return;
  free_all_stocks(root->left);
  free_all_stocks(root->right);
  free(root);
}

void sigintHandler(int sig)
{
  writeFile();
  if (global_pool)
  {
    for (int i = 0; i <= global_pool->maxi; i++)
    {
      if (global_pool->clientfd[i] > 0)
        Close(global_pool->clientfd[i]);
    }
  }
  if (global_listenfd > 0)
    Close(global_listenfd);
  free_all_stocks(head);
  free(original_order);
  exit(0);
}

void init_pool(int listenfd, pool *p)
{
  int i;
  p->maxi = -1;

  for (i = 0; i < FD_SETSIZE; i++)
    p->clientfd[i] = -1;

  p->maxfd = listenfd;
  FD_ZERO(&p->read_set);
  FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p)
{
  int i;
  p->nready--;
  for (i = 0; i < FD_SETSIZE; i++)
  {
    if (p->clientfd[i] < 0)
    {
      p->clientfd[i] = connfd;
      Rio_readinitb(&(p->clientrio[i]), connfd);

      FD_SET(connfd, &p->read_set);

      if (connfd > p->maxfd)
        p->maxfd = connfd;
      if (i > p->maxi)
        p->maxi = i;
      break;
    }
  }
  if (i == FD_SETSIZE)
    app_error("add_client error : Too many clients");
}

void check_clients(pool *p)
{
  int i, connfd, n;
  char buf[MAXLINE];
  rio_t rio;

  for (i = 0; (i <= p->maxi) && (p->nready > 0); i++)
  {
    connfd = p->clientfd[i];
    rio = p->clientrio[i];

    if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set)))
    {
      p->nready--;
      if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
      {
        byte_cnt += n;
        printf("Server received %d bytes\n", n);
        // Rio_writen(connfd, buf, n);
        int exitornot = analyze_commands(buf, connfd);
        if (exitornot == 1)
        {
          Close(connfd);
          FD_CLR(connfd, &p->read_set);
          p->clientfd[i] = -1;
          check_and_update_file(p);
          break;
        }
      }

      else
      {
        Close(connfd);
        FD_CLR(connfd, &p->read_set);
        p->clientfd[i] = -1;
        check_and_update_file(p);
      }
    }
  }
}

int analyze_commands(char *buf, int connfd)
{
  char *argv[3];
  char *ret_ptr;
  char *next_ptr;
 
  ret_ptr = strtok_r(buf, DELIM_CHARS, &next_ptr);
  int argc = 0;
  while(ret_ptr && (argc < 3)) {
    argv[argc++] = ret_ptr;
    ret_ptr = strtok_r(NULL, DELIM_CHARS, &next_ptr);
  }
  if (argc == 0) return 0;
  // show, buy, sell, exit
  if (!strncmp(argv[0], "show", 4)) {
    show_stocks(connfd);
    return 0;
  }
  if (!strncmp(argv[0], "buy", 3)) {
    int id = atoi(argv[1]);
    int amount = atoi(argv[2]);
    buy_stock(id, amount, connfd);
    return 0;
  }
  if (!strncmp(argv[0], "sell", 3)) {
    int id = atoi(argv[1]);
    int amount = atoi(argv[2]);
    sell_stock(id, amount, connfd);
    return 0;
  }
  if (!strncmp(argv[0], "exit", 4)) {
    char temp[MAXLINE];
    memset(temp, 0, MAXLINE);
    strcpy(temp, "\n");
    Rio_writen(connfd, temp, MAXLINE);
    return 1;
  }
  return 0;
}

void scanFile()
{
  FILE *stockfp = fopen("stock.txt", "r");
  if (stockfp == NULL)
  {
    perror("No stock.txt\n");
    exit(1);
  }
  int stock_count = 0;

  // ID, 잔여 주식, 주식 단가
  int stockID, left, price;
  while (fscanf(stockfp, "%d %d %d", &stockID, &left, &price) == 3)
  {
    stock_count++;
  }
  original_order = malloc(stock_count * sizeof(int));
  if (original_order == NULL)
  {
    perror("Memory allocation failure for original_order");
    fclose(stockfp);
    exit(1);
  }

  rewind(stockfp);
  int order = 0;

  while (fscanf(stockfp, "%d %d %d", &stockID, &left, &price) == 3)
  {
    head = insertStockNode(head, stockID, left, price, order);
    original_order[order++] = stockID;
    // printf("%d %d %d\n", stockID, left, price);
  }

  fclose(stockfp);
  
  return;
}

stockNode *insertStockNode(stockNode *root, int stockID, int left, int price, int order)
{
  // 비어있는 경우
  if (root == NULL)
  {
    struct stockNode *newNode = (struct stockNode *)malloc(sizeof(struct stockNode));
    if (newNode == NULL)
    {
      perror("Memory Allocation Failure\n");
      exit(1);
    }
    newNode->id = stockID;
    newNode->left_stock = left;
    newNode->price = price;
    newNode->file_order = order;
    newNode->left = NULL;
    newNode->right = NULL;
    root = newNode;
    return root;
  }

  else
  {
    if (root->id < stockID)
    {
      // 현재값이 루트 값보다 더 큼: 오른쪽으로 가자
      root->right = insertStockNode(root->right, stockID, left, price, order);
    }
    else
    {
      // 현재값이 루트 값보다 더 작음: 왼쪽으로 가자
      root->left = insertStockNode(root->left, stockID, left, price, order);
    }
  }
  return root;
}

void buy_stock(int id, int amount, int connfd)
{
  char temp[MAXLINE];
  memset(temp, 0, MAXLINE);
  stockNode* target = search_stock(head, id);
  if (target == NULL)
  {
    strcpy(temp, "Invalid stock ID\n");
  }
  else if (target->left_stock >= amount)
  {
    target->left_stock -= amount;
    strcpy(temp, "[buy] success\n");
  }
  else
  {
    strcpy(temp, "Not enough left stock\n");
  }
  Rio_writen(connfd, temp, MAXLINE);
  return;
}

void sell_stock(int id, int amount, int connfd)
{
  char temp[MAXLINE];
  memset(temp, 0, MAXLINE);
  stockNode *target = search_stock(head, id);
  if (target == NULL)
  {
    strcpy(temp, "Invalid stock ID\n");
  }
  else
  {
    target->left_stock += amount;
    strcpy(temp, "[sell] success\n");
  }
  Rio_writen(connfd, temp, MAXLINE);
  return;
}

void show_stocks(int connfd)
{
  int i = 0;
  char result[MAXLINE];
  memset(result, 0, MAXLINE);

  while (original_order[i] != 0)
  {
    stockNode *target = search_stock(head, original_order[i++]);
    if (target == NULL) continue;
    
    char line[MAXLINE];
    memset(line, 0, MAXLINE);
    
    sprintf(line, "%d %d %d\n", target->id, target->left_stock, target->price);
    strcat(result, line);
  }
  Rio_writen(connfd, result, MAXLINE);
  return;
}

stockNode* search_stock(stockNode* root, int id) {
  if (root == NULL)
    return NULL;
  else if (root->id == id)
    return root;
  else if (id < root->id)
    return search_stock(root->left, id);
  else
    return search_stock(root->right, id);
}

void check_and_update_file(pool *p)
{
  int alive_count = 0;

  for (int i = 0; i <= p->maxi; i++)
  {
    if (p->clientfd[i] > 0)
      alive_count++;
  }

  if (alive_count == 0)
  {
    writeFile();
  }
  return;
}
