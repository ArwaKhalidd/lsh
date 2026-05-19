#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

/*
  Function Declarations for builtin shell commands:
 */
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_pwd(char **args);
int lsh_echo(char **args);
int lsh_history(char **args);
int lsh_env(char **args);

/* history storage */
#define LSH_HISTORY_MAX 1000

static char *lsh_history_list[LSH_HISTORY_MAX];
static int lsh_history_count = 0;

/**
   @brief Add command line to history.
*/
static void lsh_add_history(const char *line)
{
  if (!line || line[0] == '\0') {
    return;
  }

  if (lsh_history_count < LSH_HISTORY_MAX) {
    lsh_history_list[lsh_history_count++] = strdup(line);
  } else {
    free(lsh_history_list[0]);

    /* Shift history left */
    for (int i = 1; i < LSH_HISTORY_MAX; i++) {
      lsh_history_list[i - 1] = lsh_history_list[i];
    }

    lsh_history_list[LSH_HISTORY_MAX - 1] = strdup(line);
  }
}

/**
   @brief Free history memory before exit.
*/
static void lsh_free_history(void)
{
  for (int i = 0; i < lsh_history_count; i++) {
    free(lsh_history_list[i]);
  }
}

/*
  List of builtin commands, followed by their corresponding functions.
 */
char *builtin_str[] = {
  "cd",
  "help",
  "exit",
  "pwd",
  "echo",
  "history",
  "env"
};

int (*builtin_func[]) (char **) = {
  &lsh_cd,
  &lsh_help,
  &lsh_exit,
  &lsh_pwd,
  &lsh_echo,
  &lsh_history,
  &lsh_env
};

/**
   @brief Return number of builtin commands.
*/
int lsh_num_builtins(void)
{
  return sizeof(builtin_str) / sizeof(char *);
}

/*
  Builtin function implementations.
*/

/**
   @brief Builtin command: change directory.
   @param args List of args. args[0] is "cd". args[1] is the directory.
   @return Always returns 1, to continue executing.
 */
int lsh_cd(char **args)
{
  if (args[1] == NULL) {
    fprintf(stderr, "lsh: expected argument to \"cd\"\n");
  } else if (args[2] != NULL) {
    fprintf(stderr, "lsh: cd: too many arguments\n");
  } else {
    if (chdir(args[1]) != 0) {
      perror("lsh");
    }
  }

  return 1;
}

/**
   @brief Builtin command: print help.
   @param args List of args. Not examined.
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
   @param args List of args. Not examined.
   @return Always returns 0, to terminate execution.
 */
int lsh_exit(char **args)
{
  return 0;
}

/**
   @brief Builtin command: print working directory.
*/
int lsh_pwd(char **args)
{
  char cwd[PATH_MAX];

  if (args[1] != NULL) {
    fprintf(stderr, "lsh: pwd: too many arguments\n");
    return 1;
  }

  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    printf("%s\n", cwd);
  } else {
    perror("lsh: pwd");
  }

  return 1;
}

/**
   @brief Builtin command: echo arguments.
*/
int lsh_echo(char **args)
{
  int i = 1;

  while (args[i] != NULL) {
    if (i > 1) {
      printf(" ");
    }

    printf("%s", args[i]);
    i++;
  }

  printf("\n");

  return 1;
}

/**
   @brief Builtin command: print command history.
*/
int lsh_history(char **args)
{
  for (int i = 0; i < lsh_history_count; i++) {
    printf("%d %s\n", i + 1, lsh_history_list[i]);
  }

  return 1;
}

extern char **environ;

/**
   @brief Builtin command: print environment variables.
*/
int lsh_env(char **args)
{
  for (char **env = environ; *env != NULL; env++) {
    printf("%s\n", *env);
  }

  return 1;
}

/**
   @brief Launch a program and wait for it to terminate.
   @param args Null terminated list of arguments (including program).
   @return Always returns 1, to continue execution.
 */
int lsh_launch(char **args)
{
  pid_t pid;
  int status;

  pid = fork();

  if (pid == 0) {
    /* Child process */

    if (execvp(args[0], args) == -1) {
      perror("lsh");
    }

    exit(EXIT_FAILURE);

  } else if (pid < 0) {
    /* Error forking */

    perror("lsh");

  } else {
    /* Parent process */

    do {
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

/**
   @brief Execute shell builtin or launch program.
   @param args Null terminated list of arguments.
   @return 1 if the shell should continue running, 0 if it should terminate.
 */
int lsh_execute(char **args)
{
  int i;

  if (args[0] == NULL) {
    /* Empty command */
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
#ifdef LSH_USE_STD_GETLINE

  char *line = NULL;
  ssize_t bufsize = 0;

  if (getline(&line, &bufsize, stdin) == -1) {
    if (feof(stdin)) {
      exit(EXIT_SUCCESS);
    } else {
      perror("lsh: getline");
      exit(EXIT_FAILURE);
    }
  }

  return line;

#else

#define LSH_RL_BUFSIZE 1024

  int bufsize = LSH_RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
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

    /* Reallocate if buffer exceeded */
    if (position >= bufsize) {
      bufsize += LSH_RL_BUFSIZE;

      char *buffer_backup = buffer;
      buffer = realloc(buffer, bufsize);

      if (!buffer) {
        free(buffer_backup);
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }

#endif
}

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"

/**
   @brief Split a line into tokens.
   @param line The line.
   @return Null-terminated array of tokens.
 */
char **lsh_split_line(char *line)
{
  int bufsize = LSH_TOK_BUFSIZE;
  int position = 0;

  char **tokens = malloc(bufsize * sizeof(char *));
  char **tokens_backup;
  char *token;

  if (!tokens) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, LSH_TOK_DELIM);

  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += LSH_TOK_BUFSIZE;

      tokens_backup = tokens;
      tokens = realloc(tokens, bufsize * sizeof(char *));

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

    line = lsh_read_line();

    /* Record command in history */
    lsh_add_history(line);

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
   @return Status code.
 */
int main(int argc, char **argv)
{
  /* Load config files if needed */

  /* Run command loop */
  lsh_loop();

  /* Cleanup */
  lsh_free_history();

  return EXIT_SUCCESS;
}
