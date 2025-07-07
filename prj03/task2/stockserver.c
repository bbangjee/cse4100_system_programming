#include "csapp.h"
#define DELIM_CHARS     " \n"
#define NTHREADS 100
#define SBUFSIZE 1024

typedef struct stockNode
{
  int id;
  int left_stock;
  int price;
  int file_order;
  int readcnt;
  sem_t mutex; //read_cnt 보호
  sem_t write_lock;
  struct stockNode *left;
  struct stockNode *right;
} stockNode;

typedef struct
{
  int *buf;
  int n;
  int front;
  int rear;
  sem_t mutex;
  sem_t slots;
  sem_t items;
} sbuf_t;

sbuf_t sbuf;
static int byte_cnt;
static sem_t mutex;
static int active_clients = 0;  // 클라이언트 수
static sem_t client_mutex;
stockNode *head = NULL;
int *original_order;
int global_listenfd = -1;

static void init_echo_cnt(void);
void sigintHandler(int sig);
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);
void *thread(void *vargp);
void stock_service(int connfd);
void scanFile();
int analyze_commands(char *buf, int connfd);
void buy_stock(int id, int amount, int connfd);
void show_stocks(int connfd);
void sell_stock(int id, int amount, int connfd);
void writeFile();
void free_all_stocks(stockNode *root);
stockNode *insertStockNode(stockNode *root, int stockID, int left, int price, int order);
stockNode *search_stock(stockNode *root, int id);

int main(int argc, char **argv)
{
  Signal(SIGINT, sigintHandler);

  int i, listenfd, connfd;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;

  char client_hostname[MAXLINE], client_port[MAXLINE];

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(0);
  }

  // stock.txt 읽어들이기
  scanFile();

  listenfd = Open_listenfd(argv[1]);
  Sem_init(&mutex, 0, 1);
  Sem_init(&client_mutex, 0, 1);
  sbuf_init(&sbuf, SBUFSIZE);

  for (i = 0; i < NTHREADS; i++)
  {
    Pthread_create(&tid, NULL, thread, NULL);
  }

  while (1)
  {
    clientlen = sizeof(struct sockaddr_storage);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
    printf("Connected to (%s, %s)\n", client_hostname, client_port);
    
    P(&client_mutex);
    active_clients++;
    V(&client_mutex);
    
    sbuf_insert(&sbuf, connfd);
  }

  sbuf_deinit(&sbuf);
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
  if (global_listenfd > 0)
    Close(global_listenfd);
  sbuf_deinit(&sbuf);
  free_all_stocks(head);
  free(original_order);
  exit(0);
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
    newNode->readcnt = 0;
    Sem_init(&newNode->mutex, 0, 1);  
    Sem_init(&newNode->write_lock, 0, 1);
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
  // write_lock: 다른 writer도 reader도 못 들어옴
  char temp[MAXLINE];
  memset(temp, 0, MAXLINE);
  stockNode* target = search_stock(head, id);
  if (target == NULL)
  {
    strcpy(temp, "Invalid stock ID\n");
    Rio_writen(connfd, temp, MAXLINE);
    return;
  }

  P(&target->write_lock);
  if (target->left_stock >= amount)
  {
    target->left_stock -= amount;
    strcpy(temp, "[buy] success\n");
  }
  else
  {
    strcpy(temp, "Not enough left stock\n");
  }
  V(&target->write_lock);
  Rio_writen(connfd, temp, MAXLINE);
  return;
}

void sell_stock(int id, int amount, int connfd)
{
  // write_lock: 다른 writer도 reader도 못 들어옴
  char temp[MAXLINE];
  memset(temp, 0, MAXLINE);
  stockNode *target = search_stock(head, id);
  if (target == NULL)
  {
    strcpy(temp, "Invalid stock ID\n");
    Rio_writen(connfd, temp, MAXLINE);
    return;
  }
  P(&target->write_lock);
  target->left_stock += amount;
  strcpy(temp, "[sell] success\n");
  V(&target->write_lock);

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
    
    P(&target->mutex);
    target->readcnt++;
    if (target->readcnt == 1) // 첫 번째 리더
      P(&target->write_lock); // writer 못 들어오게
    V(&target->mutex);
    
    // 읽기
    char line[MAXLINE];
    memset(line, 0, MAXLINE);
    sprintf(line, "%d %d %d\n", target->id, target->left_stock, target->price);
    strcat(result, line);

    P(&target->mutex);
    target->readcnt--;
    if (target->readcnt == 0) // 마지막 리더
      V(&target->write_lock);
    V(&target->mutex);
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

void sbuf_init(sbuf_t *sp, int n)
{
  sp->buf = Calloc(n, sizeof(int));
  sp->n = n;
  sp->front = sp->rear = 0;
  Sem_init(&sp->mutex, 0, 1);
  Sem_init(&sp->slots, 0, n);
  Sem_init(&sp->items, 0, 0);
}

void sbuf_deinit(sbuf_t *sp)
{
  Free(sp->buf);
}

void sbuf_insert(sbuf_t *sp, int item)
{
  P(&sp->slots);
  P(&sp->mutex);
  sp->buf[(++sp->rear)%(sp->n)] = item;
  V(&sp->mutex);
  V(&sp->items);
}

int sbuf_remove(sbuf_t *sp)
{
  int item;
  P(&sp->items);
  P(&sp->mutex);
  item = sp->buf[(++sp->front)%(sp->n)];
  V(&sp->mutex);
  V(&sp->slots);
  return item;
}

void *thread(void *vargp)
{
  Pthread_detach(pthread_self());
  while (1)
  {
    int connfd = sbuf_remove(&sbuf);
    // echo_cnt(connfd);
    stock_service(connfd);
    Close(connfd);
  }
}

void stock_service(int connfd)
{
  int n;
  char buf[MAXLINE];
  // char obuf[MAXLINE];
  rio_t rio;
  static pthread_once_t once = PTHREAD_ONCE_INIT;
  Pthread_once(&once, init_echo_cnt);
  Rio_readinitb(&rio, connfd);

  while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
  {

    P(&mutex);
    byte_cnt += n;
    printf("Server received %d bytes\n", n);
    V(&mutex);
    
    int exit_flag = analyze_commands(buf, connfd);
    if (exit_flag == 1) 
      break; // exit 명령어면 루프 탈출
  }

  P(&client_mutex);
  active_clients--;
  
  if (active_clients == 0)
  {
    writeFile();
  }
  V(&client_mutex);
  return;
}

static void init_echo_cnt(void) 
{
  // Sem_init(&mutex, 0, 1);
  // Sem_init(&client_mutex, 0, 1);
  byte_cnt = 0;
}