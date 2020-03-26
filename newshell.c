/***************************************************************************//**
  @file         main.c
  @author       Stephen Brennan
  @date         Thursday,  8 January 2015
  @brief        LSH (Libstephen SHell)
*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <errno.h>
#include <pwd.h>

#define LSH_RL_BUFSIZE 1024
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
#define TRUE 1
#define FALSE 0

/*
  Function Declarations for builtin shell commands:
 */
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_pwd(char **args);
int lsh_grep(char **args);
int lsh_info(char **args);
int lsh_clear(char **args);
int lsh_pipe(char**args, int left, int right);
int lsh_redi(char**args, int left, int right);

int position;

const char* COMMAND_OUT = ">";
const char* COMMAND_PIPE = "|";

enum {
	RET_SUCCESS=1,
	ERROR_FORK,
	ERROR_COMMAND,
	ERROR_MISS_PARAMETER,
	ERROR_WRONG_PARAMETER,

	/* 重定向的错误信息 */

	ERROR_MANY_OUT,
	ERROR_FILE_NOT_EXIST,
	
	/* 管道的错误信息 */
	ERROR_PIPE
};
/*
  List of builtin commands, followed by their corresponding functions.
 */
char *builtin_str[] = {
  	"cd",
  	"help",
  	"exit",
  	"pwd",
  	"grep",
  	"info",
  	"clear"
};

int (*builtin_func[]) (char **) = {
  	&lsh_cd,
  	&lsh_help,
  	&lsh_exit,
  	&lsh_pwd,
  	&lsh_grep,
  	&lsh_info,
  	&lsh_clear
};


int lsh_num_builtins() {
  	return sizeof(builtin_str) / sizeof(char *);
}

void getUsername(char* username) { // 获取当前登录的用户名
	struct passwd* pwd = getpwuid(getuid());
	strcpy(username, pwd->pw_name);
}

void getHostname(char* hostname) { // 获取主机名
	gethostname(hostname, LSH_RL_BUFSIZE);
}

int isCommandExist(const char* command) { // 判断指令是否存在
	if (command == NULL || strlen(command) == 0) return FALSE;

	int result = TRUE;
	
	int fds[2];
	if (pipe(fds) == -1) {
		result = FALSE;
	}
	

	return result;
}

char* textFileRead(const char *filename)
{
	char* text = NULL;
	FILE *pf;
	if( (pf = fopen(filename,"r")) == NULL ){
		return NULL;
	}
	fseek(pf,0,SEEK_END);
	int ISize = ftell(pf);
	text = (char*)malloc(sizeof(char)*(ISize+10));
	rewind(pf);
	fread(text,sizeof(char),ISize,pf);
	text[ISize] = '\0';
	
	fclose(pf);
	return text;
}

int strindex(char s[],char t[]){
	int found = 0;
	int i,j,k;
	for(i=0; s[i] != '\0'; i++){
		for(j=i,k=0; t[k]!='\0' && s[j]==t[k]; j++, k++)
			;
		if(k>0 && t[k]=='\0')
			found++;
	}
	return found;
}

/*
  Builtin function implementations.
*/

/**
   @brief Bultin command: change directory.
   @param args List of args.  args[0] is "cd".  args[1] is the directory.
   @return Always returns 1, to continue executing.
 */
int lsh_cd(char **args)
{
	if (args[1] == NULL) {
		fprintf(stderr, "lsh: expected argument to \"cd\"\n");
	} else {
		if (chdir(args[1]) != 0) {
		    perror("lsh");
		}
	}
	return 1;
}

/**
   @brief Builtin command: print help.
   @param args List of args.  Not examined.
   @return Always returns 1, to continue executing.
 */
int lsh_help(char **args)
{
  	int i;
  	printf("Stephen Brennan's LSH\n");
  	printf("Type program names and arguments, and hit enter.\n");
  	printf("The following are built in:\n");

  	for (i = 0; i < lsh_num_builtins(); i++) {
    	printf("  %s\n", builtin_str[i]);
  	}

  	printf("Use the man command for information on other programs.\n");
  	return 1;
}

/**
   @brief Builtin command: exit.
   @param args List of args.  Not examined.
   @return Always returns 0, to terminate execution.
 */
int lsh_exit(char **args)
{
  	return 0;
}

int lsh_pwd(char **args)
{
	char username[LSH_RL_BUFSIZE];
	char hostname[LSH_RL_BUFSIZE];
	getUsername(username);
	getHostname(hostname);
	
	char curPath[LSH_RL_BUFSIZE];
	char* result = getcwd(curPath, LSH_RL_BUFSIZE);
	if (result == NULL)
		perror("lsh");
	else{
		printf("\e[32;1m%s@%s\e[0m:\e[36;1m%s\e[0m$ ", username, hostname,curPath);
		return 1;
	}
}

int lsh_grep(char **args)
{
	if (strcmp(args[1], "-c") != 0) {
		fprintf(stderr, "ERROR: WRONG PARAMETER\n");
		return ERROR_WRONG_PARAMETER;
	}
	
	char *text = textFileRead(args[3]);
	if(text == NULL){
		fprintf(stderr, "ERROR:File don't' exist \"cd\"\n");
		return ERROR_FILE_NOT_EXIST;
	}
	
	char* pattern = args[2];
	int found = strindex(text,pattern);
	
	printf("There are %d times of the file that satisfies the pattern match.\n",found);
	free(text);
	
	return RET_SUCCESS;
	
}

int lsh_info(char **args)
{
	char username[LSH_RL_BUFSIZE];
	char hostname[LSH_RL_BUFSIZE];
	getUsername(username);
	getHostname(hostname);
	printf("XJCO2211 Simplified Shell by %s\n",username);
}

int lsh_clear(char **args)
{
    fputs("\x1b[2J\x1b[H", stdout);
    return 1;
}

/**
  @brief Launch a program and wait for it to terminate.
  @param args Null terminated list of arguments (including program).
  @return Always returns 1, to continue execution.
 */



int lsh_launch(char** args) { // 给用户使用的函数，用以执行用户输入的命令
	int number_command = position;
	pid_t pid = fork();
	if (pid == -1) {
		perror("lsh");	// Error forking
	} else if (pid == 0) {
		/* 获取标准输入、输出的文件标识符 */
		int in_file_identifier = dup(STDIN_FILENO);
		int out_file_identifier = dup(STDOUT_FILENO);

		int result = lsh_pipe(args, 0, number_command);
		
		/* 还原标准输入、输出重定向 */
		dup2(in_file_identifier, STDIN_FILENO);
		dup2(out_file_identifier, STDOUT_FILENO);
		exit(result);
	} else {
		int status;
		waitpid(pid, &status, 0);
		return WEXITSTATUS(status);
	}
}

int lsh_pipe(char**args, int left, int right) { // 所要执行的指令区间[left, right)，可能含有管道
	if (left >= right) return RET_SUCCESS;
	/* 判断是否有管道命令 */
	int pipe_position = -1;
	int i;
	for (i=left; i<right; i++) {
		if (strcmp(args[i], COMMAND_PIPE) == 0) {
			pipe_position = i;
			break;
		}
	}
	if (pipe_position == -1) { // 不含有管道命令
		return lsh_redi(args, left, right);
	} else if (pipe_position+1 == right) { // 管道命令'|'后续没有指令，参数缺失
		fprintf(stderr, "ERROR: PIPE MISS PARAMETER\n");
		return ERROR_MISS_PARAMETER;
	}

	/* 执行命令 */
	int file_identifiers[2];
	if (pipe(file_identifiers) == -1) {
		fprintf(stderr, "ERROR: PIPE FUNCTION FAILED\n");
		return ERROR_PIPE;
	}
	int result = RET_SUCCESS;
	pid_t pid = vfork();
	if (pid == -1) {
		result = ERROR_FORK;
		fprintf(stderr, "ERROR: FORK FUNCTION FAILED\n");
	} else if (pid == 0) { // 子进程执行单个命令
		close(file_identifiers[0]);
		dup2(file_identifiers[1], STDOUT_FILENO); // 将标准输出重定向到fds[1]
		close(file_identifiers[1]);
		
		result = lsh_redi(args, left, pipe_position);
		exit(result);
	} else { // 父进程递归执行后续命令
		int status;
		waitpid(pid, &status, 0);
		int exitCode = WEXITSTATUS(status);
		
		if (exitCode != RET_SUCCESS) { // 子进程的指令没有正常退出，打印错误信息
			char info[4096] = {0};
			char line[LSH_RL_BUFSIZE];
			close(file_identifiers[1]);
			dup2(file_identifiers[0], STDIN_FILENO); // 将标准输入重定向到fds[0]
			close(file_identifiers[0]);
			while(fgets(line, LSH_RL_BUFSIZE, stdin) != NULL) { // 读取子进程的错误信息
				strcat(info, line);
			}
			printf("%s", info); // 打印错误信息
			
			result = exitCode;
		} else if (pipe_position+1 < right){
			close(file_identifiers[1]);
			dup2(file_identifiers[0], STDIN_FILENO); // 将标准输入重定向到fds[0]
			close(file_identifiers[0]);
			result = lsh_pipe(args, pipe_position+1, right); // 递归执行后续指令
		}
	}

	return result;
}

int lsh_redi(char**args, int left, int right) { // 所要执行的指令区间[left, right)，不含管道，可能含有重定向
	if (!isCommandExist(args[left])) { // 指令不存在
        fprintf(stderr, "ERROR: This command not exist in myshell.\n");
		return ERROR_COMMAND;
	}	

	/* 判断是否有重定向 */
	int outNum = 0;
	char *outFile = NULL;
	int endIdx = right; // 指令在重定向前的终止下标

	for (int i=left; i<right; ++i) {
		if (strcmp(args[i], COMMAND_OUT) == 0) { // 输出重定向
			++outNum;
			if (i+1 < right)
				outFile = args[i+1];
			else{
			    fprintf(stderr, "\e[31;1mError: Miss redirect file parameters.\n\e[0m");
			    return ERROR_MISS_PARAMETER; // 重定向符号后缺少文件名
		    }
			if (endIdx == right) endIdx = i;
		}
	}
	/* 处理重定向 */
	if (outNum > 1) { // 输出重定向符超过一个
	    fprintf(stderr, "\e[31;1mError: Too many redirection symbol \\.\n\e[0m");
		return ERROR_MANY_OUT;
	}

	int result = RET_SUCCESS;
	pid_t pid = vfork();
	if (pid == -1) {
		result = ERROR_FORK;
		fprintf(stderr, "\e[31;1mError: Fork function failed.\n\e[0m");
	} else if (pid == 0) {
		/* 输出重定向 */
		if (outNum == 1)
			freopen(outFile, "w", stdout);

		/* 执行命令 */
		char* comm[LSH_RL_BUFSIZE];
		int i;
		for (i=left; i<endIdx; i++)
			comm[i] = args[i];
		comm[endIdx] = NULL;
		execvp(comm[left], comm+left);
		exit(errno); // 执行出错，返回errno
	} else {
		int status;
		waitpid(pid, &status, 0);
		int err = WEXITSTATUS(status); // 读取子进程的返回码

		if (err) { // 返回码不为0，意味着子进程执行出错，用红色字体打印出错信息
			printf("\e[31;1mError: %s\n\e[0m", strerror(err));
		}
	}


	return result;
}

/**
   @brief Execute shell built-in or launch program.
   @param args Null terminated list of arguments.
   @return 1 if the shell should continue running, 0 if it should terminate
 */
int lsh_execute(char **args)
{
  	int i;

  	if (args[0] == NULL) {
    	// An empty command was entered.
    	return 1;
  	}

  	for (i = 0; i < lsh_num_builtins(); i++) {
    	if (strcmp(args[0], builtin_str[i]) == 0) {
      		return (*builtin_func[i])(args);
    	}
  	}

  	return lsh_launch(args);
}

/**
   @brief Read a line of input from stdin.
   @return The line from stdin.
 */
char *lsh_read_line(void)
{ 
	int bufsize = LSH_RL_BUFSIZE;
  	int position = 0;
  	char *buffer = malloc(sizeof(char) * bufsize);
  	int c;

  	if (!buffer) {
    	fprintf(stderr, "lsh: allocation error\n");
    	exit(EXIT_FAILURE);
  	}

  	while (1) {
    	// Read a character
    	c = getchar();

    	if (c == EOF) {
      		exit(EXIT_SUCCESS);
    	} else if (c == '\n') {
      		buffer[position] = '\0';
      		return buffer;
    	} else {
      		buffer[position] = c;
    	}
    	position++;

    	// If we have exceeded the buffer, reallocate.
    	if (position >= bufsize) {
      		bufsize += LSH_RL_BUFSIZE;
      		buffer = realloc(buffer, bufsize);
      		if (!buffer) {
        		fprintf(stderr, "lsh: allocation error\n");
        		exit(EXIT_FAILURE);
      		}
    	}
  	}
}

/**
   @brief Split a line into tokens (very naively).
   @param line The line.
   @return Null-terminated array of tokens.
 */
char **lsh_split_line(char *line)
{
  	int bufsize = LSH_TOK_BUFSIZE;
	position = 0;
  	char **tokens = malloc(bufsize * sizeof(char*));
  	char *token, **tokens_backup;

  	if (!tokens) {
    	fprintf(stderr, "lsh: allocation error\n");
    	exit(EXIT_FAILURE);
  	}

  	token = strtok(line, LSH_TOK_DELIM);
  	for (position = 0 ; token != NULL ; ) {
    	tokens[position] = token;
    	position++;

    	if (position >= bufsize) {
      		bufsize += LSH_TOK_BUFSIZE;
      		tokens_backup = tokens;
      		tokens = realloc(tokens, bufsize * sizeof(char*));
      		if (!tokens) {
				free(tokens_backup);
        		fprintf(stderr, "lsh: allocation error\n");
        		exit(EXIT_FAILURE);
      		}
    	}

    	token = strtok(NULL, LSH_TOK_DELIM);
  	}
  	tokens[position] = NULL;
  	return tokens;
}

/**
   @brief Loop getting input and executing it.
 */
void lsh_loop(void)
{
  	char *line;
  	char **args;
  	int status;

  	do {
    	printf("> ");
    	lsh_pwd(args);
    	line = lsh_read_line();
    	args = lsh_split_line(line);
    	status = lsh_execute(args);

    	free(line);
    	free(args);
  	} while (status);
}




/**
   @brief Main entry point.
   @param argc Argument count.
   @param argv Argument vector.
   @return status code
 */
int main(int argc, char **argv)
{
  	// Load config files, if any.

  	// Run command loop.
  	lsh_loop();

  	// Perform any shutdown/cleanup.

  	return EXIT_SUCCESS;
}
