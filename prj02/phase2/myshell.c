#include <signal.h>
#include "myshell.h"

/* functions using
- main
- clear_commands
- eval
- parseline
- builtin_command

- sigchld_handler
- sigint_handler
- sigstp_handler

- add_job
- init_jobs
- unix_error
- Fork
- Wait
- Waitpid
*/

#define MAXJOBS 20
#define JOB_STATE_UNDEF -1
#define JOB_STATE_RUNNING 0
#define JOB_STATE_STOPPED 1
#define JOB_BG_UNDEF -1
#define JOB_BG_FG 0
#define JOB_BG_BG 1

// 구조체 선언
typedef struct job {
  int jid; /* job id [1] [2] 이런거*/
  pid_t pid; /* process ID */
  pid_t pgid; /* process group ID */
  char cmdline[MAXLINE];
  int state; /* Undef Running Stopped -1, 0, 1 */
  int is_bg; /* fg : 0 , bg : 1 */
} job;

// 전역 변수 선언
job jobs[MAXJOBS];
pid_t fg_pgid = 0; // fg 프로세스 그룹 ID
int pipe_cnt = 0; // 파이프 개수
int command_cnt = 0; // 명령어 개수
char *commands[10][MAXARGS];
char backup_cmdline[MAXLINE] = {0};

int add_job(pid_t pid, pid_t pgid, char *cmdline, int state, int is_bg) {
  if (pid < 1) return -1; // 에러

  int used_jids[MAXJOBS + 1] = {0};
  for (int i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid != 0 && jobs[i].state != JOB_STATE_UNDEF) {
      used_jids[jobs[i].jid] = 1; // jid가 사용 중임을 표시
    }
  }

  int new_jid = 1;
  for (int j = 1; j <= MAXJOBS; j++) {
    if (!used_jids[j]) {
      new_jid = j;
      break;
    }
  }

  for (int i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid == pid) return -1; // 이미 존재하는 pid

    if ((jobs[i].pid == 0) && (jobs[i].state == JOB_STATE_UNDEF) && (jobs[i].is_bg == JOB_BG_UNDEF)) {
      jobs[i].pid = pid;
      jobs[i].pgid = pgid;
      jobs[i].jid = new_jid;
      jobs[i].state = state;
      strncpy(jobs[i].cmdline, cmdline, MAXLINE);
      jobs[i].cmdline[MAXLINE - 1] = '\0';
      jobs[i].is_bg = is_bg;
      return 1;
    }
  }
  return -1; // Job table이 가득 찼음 ㅠㅠ
}

int delete_job(pid_t pid) {
  if (pid < 1) return -1;
  for (int i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid == pid) {
      jobs[i].jid = 0;
      jobs[i].pgid = 0;
      jobs[i].pid = 0;
      memset(jobs[i].cmdline, 0, sizeof(jobs[i].cmdline));
      jobs[i].state = JOB_STATE_UNDEF;
      jobs[i].is_bg = JOB_BG_UNDEF;
      return 1;
    }
  }
  return -1;
}

void print_jobs() {
  for (int i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid != 0 && jobs[i].is_bg != JOB_BG_UNDEF) {
      char *state_str;
      if (jobs[i].state == JOB_STATE_RUNNING)
        state_str = "Running";
      else if (jobs[i].state == JOB_STATE_STOPPED)
        state_str = "Stopped";
        
      printf("[%d]   %s\t\t%s\n", jobs[i].jid, state_str, jobs[i].cmdline);
    }
  }
}

void init_jobs() {
  for (int i = 0; i < MAXJOBS; i++) {
    jobs[i].jid = 0;
    jobs[i].pid = 0;
    jobs[i].pgid = 0;
    memset(jobs[i].cmdline, 0, sizeof(jobs[i].cmdline));
    jobs[i].state = JOB_STATE_UNDEF;
    jobs[i].is_bg = JOB_BG_UNDEF;
  }
  return;
}

int main() 
{
  signal(SIGCHLD, sigchld_handler);
  signal(SIGTSTP, sigtstp_handler);
  signal(SIGINT, sigint_handler);
  init_jobs();

  char cmdline[MAXLINE];
  while (1) {
	  printf("CSE4100-SP-P2> ");                   
	  char *pChar = fgets(cmdline, MAXLINE, stdin); 
	  if (feof(stdin)) exit(0);
	  clear_commands();
    eval(cmdline);
  }
  return 0;
}
  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
  sigset_t child_mask, prev, mask_all;
  sigemptyset(&child_mask);
  sigaddset(&child_mask, SIGCHLD);
  sigfillset(&mask_all);

  char buf[MAXLINE];   /* 수정된 명령어를 위한 버퍼 */
  int bg;              /* 백그라운드 여부: 1이면 bg, 0이면 fg */
  pid_t pid;           /* Process id */
  
  strcpy(buf, cmdline); // 명령어 라인을 받아 buf에 복사
  bg = parseline(buf);  // buf를 파싱
  // 만약 마지막 인자가 &이면, bg를 1로 설정, 아니면 0으로 설정
  // 아무런 입력이 없는 경우 1을 Return

  if (commands[0][0] == NULL)  return; // 빈 명령어라면 종료

  
  // 프로세스가 하나
  if (pipe_cnt == 0) {
    if (!builtin_command(commands[0])) {
      // 내장 명령어가 아님
      // quit -> exit(0), & -> ignore, other -> run
      sigprocmask(SIG_BLOCK, &child_mask, &prev);
      if ((pid = Fork()) == 0) {
        sigprocmask(SIG_SETMASK, &prev, NULL);
        setpgid(0, 0);
        if (execvp(commands[0][0], commands[0]) < 0) {	//ex) /bin/ls ls -al &
          printf("%s: command not found\n", commands[0][0]);
          exit(0);
        }
      }
  
	    // fg일 경우
      if (!bg){
        fg_pgid = pid;
	      int status;
        Waitpid(pid, &status, WUNTRACED);
        // WUNTRACED 옵션 사용으로 SIGTSTP, SIGSTOP 시그널이 자식에게 갔을 떄도 작동
        if (WIFSTOPPED(status)) {
          sigprocmask(SIG_BLOCK, &mask_all, NULL);
          // CTRL + Z 받고 중지되었을 경우 (SIGTSTOP)
          add_job(fg_pgid, fg_pgid, backup_cmdline, JOB_STATE_STOPPED, JOB_BG_FG);
          int jid = -1;
          for (int i = 0; i < MAXJOBS; i++) {
            if (jobs[i].pgid == fg_pgid) {
              jid = jobs[i].jid;
              break;
            }
          }
          if (jid != -1) {
            printf("\n[%d]   Stopped                 %s\n", jid, backup_cmdline);
          }
        }
        fg_pgid = 0;
        safe_memset(backup_cmdline, 0, sizeof(backup_cmdline));
        sigprocmask(SIG_SETMASK, &prev, NULL);
	    }
	    else {
        sigprocmask(SIG_BLOCK, &mask_all, NULL);
        add_job(pid, pid, backup_cmdline, JOB_STATE_RUNNING, JOB_BG_BG);
        int jid = -1;
        for (int i = 0; i < MAXJOBS; i++) {
          if (jobs[i].pgid == pid) {
            jid = jobs[i].jid;
            break;
          }
        }

        // 메시지 출력
        if (jid != -1) {
          printf("[%d] %d\n", jid, pid);
        }

        sigprocmask(SIG_SETMASK, &prev, NULL);
      }
    }
  }

  else {
    // pipe 및 pid_list 동적 할당
    command_cnt = pipe_cnt + 1; // 명령어 개수는 파이프 개수 + 1
    // pid 목록 배열 할당
    pid_t *pid_list = (pid_t *)malloc(sizeof(pid_t) * command_cnt);
    // 메모리 할당 실패시 pid_list에 대한 예외 처리
    if (pid_list == NULL) return;
    // file descriptor 배열 할당
    int **fd = (int **)malloc(sizeof(int *) * pipe_cnt);
    // 메모리 할당 실패시 fd에 대한 예외 처리
    if (fd == NULL) {
      free(pid_list); // pid_list도 free해줘야함
      return;
    }
    // 파이프 생성
    for (int i = 0; i < pipe_cnt; i++) {
      fd[i] = (int *)malloc(sizeof(int) * 2);
      if (fd[i] == NULL) {
        for (int j = 0; j < i; j++) {
          free(fd[j]);
        }
        free(fd);
        free(pid_list);
        return;
      }
      if (pipe(fd[i]) < 0) {
        // pipe 생성 에러 발생
        for (int j = 0; j < pipe_cnt; j++) {
          close(fd[j][0]);
          close(fd[j][1]);
          free(fd[j]);
        }
        free(fd);
        free(pid_list);
        return;
      }
    }
    
    sigprocmask(SIG_BLOCK, &child_mask, &prev);
    // 각 명령어에 대한 파이프 및 프로세스 생성
    for (int i = 0; i < command_cnt; i++) {
      if ((pid_list[i] = Fork()) == 0) {
        sigprocmask(SIG_SETMASK, &prev, NULL);
        if (commands[i][0] == NULL) {
          // fprintf(stderr, "Error: command[%d] is NULL\n", i);
          exit(1);
        }
        /*
        STDIN_FILENO(0) : 표준 입력
        STDOUT_FILENO(1): 표준 출력
        STDERR_FILENO(2): 표준 오류
        */
        if (i == 0) {
          // 첫번째 프로세스를 그룹리더로
          setpgid(0, 0);
        }
        else {
          setpgid(0, pid_list[0]);
        }
        // 첫번쨰 명렁어가 아님
        if (i > 0) {
          // 표준입력을 이전 파이프애서 읽기
          if (dup2(fd[i-1][0], STDIN_FILENO) < 0) {
            exit(1);
          }
        }

        if (i < pipe_cnt) {
          // 표준 출력을 다음 파이프의 쓰기
          if (dup2(fd[i][1], STDOUT_FILENO) < 0) {
            exit(1);
          }
        }

        for (int j = 0; j < pipe_cnt; j++) {
          close(fd[j][0]);
          close(fd[j][1]);
        }

        if (execvp(commands[i][0], commands[i]) < 0) {
          printf("%s: command not found\n", commands[i][0]);
          close(STDOUT_FILENO);
          exit(1);
        }
      }
    }
    for (int i = 0; i < pipe_cnt; i++) {
      close(fd[i][0]);
      close(fd[i][1]);
    }
    if (!bg) {
      fg_pgid = pid_list[0];
      for (int i = 0; i < command_cnt; i++) {
        setpgid(pid_list[i], fg_pgid);
      }
      
      int status;
      // Waitpid(pid_list[i], &status, WUNTRACED);
      for (int i = 0; i < command_cnt; i++) {
        if (waitpid(pid_list[i], &status, WUNTRACED) < 0) {
          printf("waitpid error: %s (errno: %d)\n", strerror(errno), errno);
        }
        
      }
      
      if (WIFSTOPPED(status)) {
        sigprocmask(SIG_BLOCK, &mask_all, NULL);
        // CTRL + Z 받고 중지되었을 경우 (SIGTSTOP)
        add_job(fg_pgid, fg_pgid, backup_cmdline, JOB_STATE_STOPPED, JOB_BG_FG);
        int jid = -1;
        for (int i = 0; i < MAXJOBS; i++) {
          if (jobs[i].pgid == fg_pgid) {
            jid = jobs[i].jid;
            break;
          }
        }
        if (jid != -1) {
          printf("\n[%d]   Stopped                 %s\n", jid, backup_cmdline);
        }
      }
      fg_pgid = 0;
      safe_memset(backup_cmdline, 0, sizeof(backup_cmdline));
      sigprocmask(SIG_SETMASK, &prev, NULL);
    }
    
    else {
      sigprocmask(SIG_BLOCK, &mask_all, NULL);
      int bg_pgid = pid_list[0];
      for (int i = 0; i < command_cnt; i++) {
        setpgid(pid_list[i], bg_pgid);
      }

      // sigprocmask(SIG_BLOCK, &mask_all, NULL);
      add_job(bg_pgid, bg_pgid, backup_cmdline, JOB_STATE_RUNNING, JOB_BG_BG);
      int jid = -1;
      for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pgid == bg_pgid) {
          jid = jobs[i].jid;
          break;
        }
      }

      // 메시지 출력
      if (jid != -1) {
        printf("[%d] %d\n", jid, bg_pgid);
      }

      sigprocmask(SIG_SETMASK, &prev, NULL);
    }

    for (int i = 0; i < pipe_cnt; i++) {
      free(fd[i]);
    }
    free(fd);
    free(pid_list);
  }
  return;
}
/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
  // exit이니까 종료
  if (!strcmp(argv[0], "exit")) {
    sigset_t mask, prev;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev);

    for (int i = 0; i < MAXJOBS; i++) {
      if (jobs[i].pid != 0 && jobs[i].state != JOB_STATE_UNDEF && jobs[i].is_bg == JOB_BG_BG) {
        kill(-jobs[i].pid, SIGKILL);
        delete_job(jobs[i].pid);
      }
    }
    sigprocmask(SIG_SETMASK, &prev, NULL);
    exit(0);
  }

  if (!strcmp(argv[0], "&")) return 1;

  if (!strcmp(argv[0], "cd")) {
    char *dir = argv[1];
    // home directory로 가야하는 경우
    if ((argv[1] == NULL) || (strcmp(argv[1], "~") == 0)) {
      dir = getenv("HOME");
      if (chdir(dir) < 0) perror("cd");
    }
    else {
      if (chdir(argv[1]) < 0) {  // 현재 프로세스의 디렉토리를 변경
        perror("cd");
      }    	
    }
    return 1;
  }
  
  if (!strcmp(argv[0], "jobs")) {
    print_jobs();
    return 1;
  }
  if (!strcmp(argv[0], "bg")) {
    to_bg(argv);
    return 1;
  }
  if (!strcmp(argv[0], "fg")) {
    to_fg(argv);
    return 1;
  } 
  
  if (!strcmp(argv[0], "kill")) {
    do_kill(argv);
    return 1;
  }
  return 0;                     /* Not a builtin command */
}
/* $end eval */


/* parseline - Parse the command line and build the commands array */
int parseline(char *buf) 
{
  pipe_cnt = 0;
  int bg = 0;              // background 여부
  
  char *temp[MAXARGS];
  char raw_cmdline[MAXLINE];

  // 개행 문자 \n을 공백으로 변환
  if (buf[strlen(buf)-1] == '\n') {
    buf[strlen(buf)-1] = '\0';
  }

  // 선행 공백 제거
  while (*buf && (*buf == ' ')) buf++;
  strcpy(raw_cmdline, buf);
  // 파이프를 기준으로 명령어 분리
  if (strchr(buf, '|') == NULL) {
    // 파이프가 없는 경우
    temp[0] = buf;
    pipe_cnt = 0;
  }

  else {
    char* token = strtok(buf, "|");
    while (token != NULL && pipe_cnt < 10) {
      temp[pipe_cnt++] = token;
      token = strtok(NULL, "|");
    }
    pipe_cnt--;
  }

  for (int i = 0; i <= pipe_cnt; i++) {
    char *command = temp[i];
    int argc = 0; 

    while (*command && (*command == ' ')) /* Ignore spaces */
      command++;
    
    char *ptr = command;
      
    while (*ptr != '\0') {
      while (*ptr == ' ') ptr++; // 공백 스킵
      if (*ptr == '\0') break;
      
      char *start;
        
      if (*ptr == '"' || *ptr == '\'') {
        char meet = *ptr;
        start = ++ptr; // 처음 문자 " or ' 무시 위해서
        
        while (*ptr && *ptr != meet) ptr++;
        if (*ptr == meet)
          *ptr = '\0'; // 끝 따옴표 null로 대체
        commands[i][argc++] = start;
        ptr++;
      }

      else {
        start = ptr;
        while (*ptr && *ptr != ' ')
          ptr++;
        if (*ptr)
          *ptr = '\0';
        commands[i][argc++] = start;
        ptr++;
      }
    }
    
    commands[i][argc] = NULL;

    if (i == pipe_cnt && argc > 0) {
      char *final_arg = commands[i][argc - 1];
      int len = strlen(final_arg);

      if (!strcmp(final_arg, "&")) {
        bg = 1;
        commands[i][--argc] = NULL;
      }
      else if (len > 0 && final_arg[len - 1] == '&'){
        bg = 1;
        final_arg[len - 1] = '\0';
      }
    }
  }
  
  memset(backup_cmdline, 0, sizeof(backup_cmdline));
  strcpy(backup_cmdline, raw_cmdline);
  
  return bg; // 백그라운드 실행 여부
}

/* unix_error_start */
void unix_error(char *msg) {
  fprintf(stderr, "%s: %s\n", msg, strerror(errno));
  exit(1);
}
/* unix_error_end */

/* Fork_Wrapper_Start */
pid_t Fork(void){
  pid_t pid = fork();
  if (pid == -1){
    unix_error("Fork Error");
  }
  return pid;
}
/* Fork_Wrapper_End */

/* Wait_Wrapper_Start */
// Wait
pid_t Wait(int *status) 
{
    pid_t pid;
    if ((pid  = wait(status)) < 0) unix_error("Wait error");
    return pid;
}
// Waitpid
pid_t Waitpid(pid_t pid, int *iptr, int options) 
{
  pid_t retpid;
  if ((retpid  = waitpid(pid, iptr, options)) < 0) unix_error("Waitpid error");
  return(retpid);
}
/* Wait_Wrapper_End */

/* Clear_commands: 명령어 배열과 카운터 초기화 */
void clear_commands() {
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < MAXARGS; j++) {
      commands[i][j] = NULL;
    }
  }
  pipe_cnt = 0;
  command_cnt = 0;
}

void do_kill(char **argv) {
  if (argv[1] == NULL) {
    printf("kill: usage: kill [-s sigspec | -n signum | -sigspec]pid | jobspec ... or kill -l [sigspec]\n");
    return;
  }
  int jid = atoi(&argv[1][1]);
  int get_pgid = 0;
  char print_cmdline[MAXLINE];
  
  for (int i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid != 0 && jobs[i].jid == jid) {
      get_pgid = jobs[i].pgid;
      strcpy(print_cmdline, jobs[i].cmdline);
      break;
    }
  }
  if (get_pgid == 0) {
    printf("kill: %s: no such job\n", argv[1]);
    return;
  }
  delete_job(get_pgid);
  if (kill(-get_pgid, SIGINT) < 0)
    perror("kill");
  printf("[%d]   Terminated              %s\n", jid, print_cmdline);
  return;
}

void sigchld_handler(int sig) {
  int old_errno = errno;
  int status;
  pid_t pid;
  
  sigset_t mask_all, prev;
  sigfillset(&mask_all);

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    sigprocmask(SIG_BLOCK, &mask_all, &prev);
    
    // fg 프로세스는 eval()에서 처리
    if (pid == fg_pgid && WIFSIGNALED(status)) {
      sigprocmask(SIG_SETMASK, &prev, NULL);
      continue;
    }
    
    int job_idx = -1;
    for (int i = 0; i < MAXJOBS; i++) {
      if (jobs[i].pid == pid) {
        job_idx = i;
        break;
      }
    }
    
    if (job_idx == -1) {
      // Job이 없는 경우 (안전 처리)
      sigprocmask(SIG_SETMASK, &prev, NULL);
      continue;
    }
    
    if (WIFEXITED(status)) {
      // bg 프로세스가 정상 종료
      delete_job(pid);
    }
    else if (WIFSIGNALED(status)) {
      // bg 프로세스가 SIGINT로 종료 (예: kill %1)
      // printf("[%d]   Terminated              %s\n", jobs[job_idx].jid, jobs[job_idx].cmdline);
      delete_job(pid);
    }
    
    sigprocmask(SIG_SETMASK, &prev, NULL);
  }

  errno = old_errno;
}

void sigint_handler(int sig) {
  sigset_t mask_all, prev;
  sigfillset(&mask_all);
  
  if (fg_pgid != 0) {
    kill(-fg_pgid, SIGINT);
    sio_puts("\n");
    sigprocmask(SIG_BLOCK, &mask_all, &prev);
    fg_pgid = 0;
    safe_memset(backup_cmdline, 0, sizeof(backup_cmdline));
    sigprocmask(SIG_SETMASK, &prev, NULL); 
  }
}

void sigtstp_handler(int sig) {
  sigset_t mask_all, prev;
  sigfillset(&mask_all);
  
  if (fg_pgid != 0) {
    kill(-fg_pgid, SIGTSTP);
  }
  
}

void to_fg(char **argv) {
  if (argv[1] == NULL) {
    printf("Usage: %s <job number>\n", argv[0]);
    return;
  }


  int jid = atoi(&argv[1][1]);
  int get_pgid = 0;
  char print_cmdline[MAXLINE];

  sigset_t mask, prev;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD); // SIGCHLD만 차단하여 경쟁 조건 방지
  sigprocmask(SIG_BLOCK, &mask, &prev);
  int job_idx;
  for (int i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid != 0 && jobs[i].jid == jid) {
      get_pgid = jobs[i].pgid;
      strcpy(print_cmdline, jobs[i].cmdline);
      job_idx = i;
      break;
    }
  }
  if (get_pgid == 0) {
    printf("fg: %%%d: no such job\n", jid);
    return;
  }
  // fg job도 stop된 상태라면 다시 foreground로
  // bg job도 stop된 상태거나 돌아가고 있는 상태거나 foreground로
  fg_pgid = get_pgid;
  if ((jobs[job_idx].is_bg == JOB_BG_BG) || ((jobs[job_idx].is_bg == JOB_BG_FG) && (jobs[job_idx].state == JOB_STATE_STOPPED))) {
    jobs[job_idx].is_bg = JOB_BG_FG;
    jobs[job_idx].state = JOB_STATE_RUNNING;
    kill(-fg_pgid, SIGCONT); // 중단된 프로세스를 계속 진행하도록 신호 보내기
  }
  /*
  sigset_t mask_all, prev;
  sigfillset(&mask_all);

  sigprocmask(SIG_BLOCK, &mask_all, &prev);
  */

  int status;
  pid_t pid = Waitpid(-get_pgid, &status, WUNTRACED); // 그룹 전체를 대기

  sigprocmask(SIG_SETMASK, &prev, NULL);
  
  if (WIFSTOPPED(status)) {
    // 작업이 중지된 경우 (예: Ctrl+Z)
    jobs[job_idx].state = JOB_STATE_STOPPED;
    printf("\n[%d]   stopped                 %s\n", jid, print_cmdline);
    fg_pgid = 0;
  }
  else if (WIFEXITED(status) || WIFSIGNALED(status)) {
    // 작업이 종료된 경우
    delete_job(get_pgid); // 작업 목록에서 제거
    fg_pgid = 0;          // 포그라운드 초기화
    safe_memset(backup_cmdline, 0, sizeof(backup_cmdline));
  }

  // 시그널 마스크 복원
  sigprocmask(SIG_SETMASK, &prev, NULL);
}

void to_bg(char **argv) {
  if (!argv[1]) {
    printf("Usage: %s <job number>\n", argv[0]);
    return;
  }
  sigset_t mask, prev;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD); // SIGCHLD만 차단하여 경쟁 조건 방지
  sigprocmask(SIG_BLOCK, &mask, &prev);

  int jid = atoi(&argv[1][1]);
  int get_pgid = 0;
  char print_cmdline[MAXLINE];
  int job_idx;

  for (int i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid != 0 && jobs[i].jid == jid) {
      get_pgid = jobs[i].pgid;
      strcpy(print_cmdline, jobs[i].cmdline);
      job_idx = i;
      break;
    }
  }

  if (get_pgid == 0) {
    printf("bg: %%%d: no such job\n", jid);
    return;
  }
  if ((jobs[job_idx].is_bg == JOB_BG_BG) && (jobs[job_idx].state == JOB_STATE_RUNNING)){
    printf("bg: job %d already in background\n", jid);
    return;
  }
  // bg ⟨job⟩: Change a stopped background job to a running background job.
  if ((jobs[job_idx].is_bg == JOB_BG_FG) || ((jobs[job_idx].is_bg == JOB_BG_BG) && (jobs[job_idx].state == JOB_STATE_STOPPED))) {
    jobs[job_idx].is_bg = JOB_BG_BG;
    jobs[job_idx].state = JOB_STATE_RUNNING;
    kill(-get_pgid, SIGCONT); // 중단된 프로세스를 계속 진행하도록 신호 보내기
  }
  sigprocmask(SIG_SETMASK, &prev, NULL);

}

/* safe_memset - safe한 memset */
void safe_memset(char *buf, char value, size_t size) {
  for (size_t i = 0; i < size; i++)
    buf[i] = value;
}
/* handler 안에서 출력하기 위한 함수들 */
static void sio_reverse(char s[])
{
  int c, i, j;

  for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
    c = s[i];
    s[i] = s[j];
    s[j] = c;
  }
}

/* sio_ltoa - long type을 string으로 변환 시켜줌 */
static void sio_ltoa(long v, char s[], int b) 
{
  int c, i = 0;
    
  do {  
    s[i++] = ((c = (v % b)) < 10)  ?  c + '0' : c - 10 + 'a';
  } while ((v /= b) > 0);
  s[i] = '\0';
  sio_reverse(s);
}

/* sio_strlen - string 길이 반환 */
static size_t sio_strlen(char s[])
{
  int i = 0;

  while (s[i] != '\0')
    ++i;
  return i;
}
/* $end sioprivate */

/* sio_puts - string 출력 */
ssize_t sio_puts(char s[]) /* Put string */
{
  return write(STDOUT_FILENO, s, sio_strlen(s)); //line:csapp:siostrlen
}
/* sio_putl - long 출력 */
ssize_t sio_putl(long v) /* Put long */
{
  char s[128];
    
  sio_ltoa(v, s, 10); /* Based on K&R itoa() */  //line:csapp:sioltoa
  return sio_puts(s);
}
/* sio_error */
void sio_error(char s[]) /* Put error message and exit */
{
  sio_puts(s);
  _exit(1);                                      //line:csapp:sioexit
}