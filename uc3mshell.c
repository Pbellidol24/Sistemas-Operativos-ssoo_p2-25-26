#include "mycalc.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

const int max_line = 1024;
const int max_commands = 10;
#define max_redirections 3 // stdin, stdout, stderr
#define max_args 15

/* Variables globales usadas por el parser/ejecutor */
char *argvv[max_args];
char *filev[max_redirections];
char *argvv_comandos[10][max_args];
int background = 0;
int redireccion_invalida = 0;

static void recolectar_zombies(void) {
  while (waitpid(-1, NULL, WNOHANG) > 0) {
  }
}

static void quitar_espacios_finales(char *texto) {
  int len = (int)strlen(texto);

  while (len > 0 && (texto[len - 1] == ' ' || texto[len - 1] == '\t' ||
                     texto[len - 1] == '\r' || texto[len - 1] == '\n')) {
    texto[len - 1] = '\0';
    len--;
  }
}

static int detectar_background(char *ultimo_comando) {
  int len = 0;

  quitar_espacios_finales(ultimo_comando);
  len = (int)strlen(ultimo_comando);
  if (len > 0 && ultimo_comando[len - 1] == '&') {
    ultimo_comando[len - 1] = '\0';
    quitar_espacios_finales(ultimo_comando);
    return 1;
  }

  return 0;
}

static int pipes_mal_formados(const char *linea) {
  int hay_token = 0;

  for (int i = 0; linea[i] != '\0'; i++) {
    if (linea[i] == ' ' || linea[i] == '\t' || linea[i] == '\n' ||
        linea[i] == '\r') {
      continue;
    }

    if (linea[i] == '|') {
      if (hay_token == 0) {
        return 1;
      }
      hay_token = 0;
    } else {
      hay_token = 1;
    }
  }

  if (hay_token == 0) {
    return 1;
  }

  return 0;
}

static int contar_args(char **args) {
  int n = 0;

  while (args[n] != NULL) {
    n++;
  }

  return n;
}

static int parsear_entero(const char *texto, int *valor) {
  long numero = 0;
  char *endptr = NULL;

  errno = 0;
  numero = strtol(texto, &endptr, 10);
  if (errno != 0 || endptr == texto || *endptr != '\0') {
    return -1;
  }

  if (numero < INT_MIN || numero > INT_MAX) {
    return -1;
  }

  *valor = (int)numero;
  return 0;
}

static int ejecutar_exit(char **args, int script_fd) {
  int argc_cmd = contar_args(args);
  int exit_code = 0;

  if (argc_cmd < 2) {
    fprintf(stderr, "[ERROR] Falta código de salida\n");
    return 0;
  }

  if (argc_cmd != 2 || parsear_entero(args[1], &exit_code) < 0) {
    fprintf(stderr, "[ERROR] El código de salida debe ser un número entero\n");
    return 0;
  }

  while (1) {
    pid_t ret = waitpid(-1, NULL, 0);

    if (ret > 0) {
      continue;
    }

    if (ret == -1 && errno == EINTR) {
      continue;
    }

    if (ret == -1 && errno == ECHILD) {
      break;
    }

    perror("waitpid");
    return -1;
  }

  printf("Goodbye %d\n", exit_code);

  if (close(script_fd) < 0) {
    perror("close");
    return -1;
  }

  exit(exit_code);
}

static int manejar_comando_interno(char **args, int script_fd) {
  int argc_cmd = 0;

  if (args[0] == NULL) {
    return 1;
  }

  if (strcmp(args[0], "exit") == 0) {
    return ejecutar_exit(args, script_fd);
  }

  if (strcmp(args[0], "mycalc") == 0) {
    argc_cmd = contar_args(args);
    (void)mycalc(argc_cmd, args);
    return 1;
  }

  return 0;
}

static void ejecutar_externo(char **args) {
  execvp(args[0], args);

  if (strcmp(args[0], "mycp") == 0) {
    execv("./mycp", args);
  }

  perror("execvp");
}

static int ejecutar_comando_simple(char **args) {
  pid_t pid = 0;

  if (args[0] == NULL) {
    return 0;
  }

  pid = fork();
  if (pid < 0) {
    perror("fork");
    return -1;
  }

  if (pid == 0) {
    if (filev[0] != NULL) {
      int fd_in = open(filev[0], O_RDONLY);
      if (fd_in < 0) {
        perror("open");
        _exit(1);
      }
      if (dup2(fd_in, STDIN_FILENO) < 0) {
        perror("dup2");
        close(fd_in);
        _exit(1);
      }
      if (close(fd_in) < 0) {
        perror("close");
        _exit(1);
      }
    }

    if (filev[1] != NULL) {
      int fd_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
      if (fd_out < 0) {
        perror("open");
        _exit(1);
      }
      if (dup2(fd_out, STDOUT_FILENO) < 0) {
        perror("dup2");
        close(fd_out);
        _exit(1);
      }
      if (close(fd_out) < 0) {
        perror("close");
        _exit(1);
      }
    }

    if (filev[2] != NULL) {
      int fd_err = open(filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
      if (fd_err < 0) {
        perror("open");
        _exit(1);
      }
      if (dup2(fd_err, STDERR_FILENO) < 0) {
        perror("dup2");
        close(fd_err);
        _exit(1);
      }
      if (close(fd_err) < 0) {
        perror("close");
        _exit(1);
      }
    }

    ejecutar_externo(args);
    _exit(1);
  }

  if (background == 1) {
    printf("%d", (int)pid);
    fflush(stdout);
  } else {
    if (waitpid(pid, NULL, 0) < 0) {
      perror("waitpid");
      return -1;
    }
    recolectar_zombies();
  }

  return 0;
}

static int ejecutar_secuencia_pipes(int num_comandos) {
  int pipefd[9][2];
  pid_t pids[10];
  int creados = 0;

  if (num_comandos < 2 || num_comandos > 3) {
    return 0;
  }

  for (int i = 0; i < num_comandos - 1; i++) {
    if (pipe(pipefd[i]) < 0) {
      perror("pipe");
      return -1;
    }
  }

  for (int i = 0; i < num_comandos; i++) {
    pids[i] = fork();
    if (pids[i] < 0) {
      perror("fork");

      for (int j = 0; j < num_comandos - 1; j++) {
        (void)close(pipefd[j][0]);
        (void)close(pipefd[j][1]);
      }

      for (int j = 0; j < creados; j++) {
        (void)waitpid(pids[j], NULL, 0);
      }

      return -1;
    }

    creados++;

    if (pids[i] == 0) {
      if (i > 0) {
        if (dup2(pipefd[i - 1][0], STDIN_FILENO) < 0) {
          perror("dup2");
          _exit(1);
        }
      }

      if (i < num_comandos - 1) {
        if (dup2(pipefd[i][1], STDOUT_FILENO) < 0) {
          perror("dup2");
          _exit(1);
        }
      }

      if (i == 0 && filev[0] != NULL) {
        int fd_in = open(filev[0], O_RDONLY);
        if (fd_in < 0) {
          perror("open");
          _exit(1);
        }
        if (dup2(fd_in, STDIN_FILENO) < 0) {
          perror("dup2");
          close(fd_in);
          _exit(1);
        }
        if (close(fd_in) < 0) {
          perror("close");
          _exit(1);
        }
      }

      if (i == num_comandos - 1 && filev[1] != NULL) {
        int fd_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd_out < 0) {
          perror("open");
          _exit(1);
        }
        if (dup2(fd_out, STDOUT_FILENO) < 0) {
          perror("dup2");
          close(fd_out);
          _exit(1);
        }
        if (close(fd_out) < 0) {
          perror("close");
          _exit(1);
        }
      }

      if (i == num_comandos - 1 && filev[2] != NULL) {
        int fd_err = open(filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd_err < 0) {
          perror("open");
          _exit(1);
        }
        if (dup2(fd_err, STDERR_FILENO) < 0) {
          perror("dup2");
          close(fd_err);
          _exit(1);
        }
        if (close(fd_err) < 0) {
          perror("close");
          _exit(1);
        }
      }

      for (int j = 0; j < num_comandos - 1; j++) {
        if (close(pipefd[j][0]) < 0) {
          perror("close");
          _exit(1);
        }
        if (close(pipefd[j][1]) < 0) {
          perror("close");
          _exit(1);
        }
      }

      ejecutar_externo(argvv_comandos[i]);
      _exit(1);
    }
  }

  for (int i = 0; i < num_comandos - 1; i++) {
    if (close(pipefd[i][0]) < 0) {
      perror("close");
      return -1;
    }
    if (close(pipefd[i][1]) < 0) {
      perror("close");
      return -1;
    }
  }

  if (background == 1) {
    printf("%d", (int)pids[num_comandos - 1]);
    fflush(stdout);
  } else {
    for (int i = 0; i < num_comandos; i++) {
      if (waitpid(pids[i], NULL, 0) < 0) {
        perror("waitpid");
        return -1;
      }
    }
    recolectar_zombies();
  }

  return 0;
}

static int linea_ignorable(const char *linea) {
  int i = 0;

  while (linea[i] == ' ' || linea[i] == '\t' || linea[i] == '\r') {
    i++;
  }

  if (linea[i] == '\0' || linea[i] == '#') {
    return 1;
  }

  return 0;
}

/**
 * This function splits a char* line into different tokens based on a given
 * character
 * @return Number of tokens
 */
int tokenizar_linea(char *linea, char *delim, char *tokens[], int max_tokens) {
  int i = 0;
  char *token = strtok(linea, delim);
  while (token != NULL && i < max_tokens - 1) {
    tokens[i++] = token;
    token = strtok(NULL, delim);
  }
  tokens[i] = NULL;
  return i;
}

static int tokenizar_argumentos(char *linea, char *tokens[], int max_tokens) {
  int i = 0;
  char *p = linea;

  while (*p != '\0' && i < max_tokens - 1) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
      p++;
    }

    if (*p == '\0') {
      break;
    }

    if (*p == '"') {
      p++;
      tokens[i++] = p;
      while (*p != '\0' && *p != '"') {
        p++;
      }
      if (*p == '"') {
        *p = '\0';
        p++;
      }
    } else {
      tokens[i++] = p;
      while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n' &&
             *p != '\r') {
        p++;
      }
      if (*p != '\0') {
        *p = '\0';
        p++;
      }
    }
  }

  tokens[i] = NULL;
  return i;
}

/**
 * This function processes the command line to evaluate if there are
 * redirections. If any redirection is detected, the destination file is
 * indicated in filev[i] array. filev[0] for STDIN filev[1] for STDOUT filev[2]
 * for STDERR
 */
void procesar_redirecciones(char *args[]) {
  int i = 0, first_red = -1;

  // Guarda el nombre de fichero asociado a cada redirección.
  for (i = 0; args[i] != NULL; i++) {

    if (strcmp(args[i], "<") == 0) {
      if (args[i + 1] == NULL) {
        redireccion_invalida = 1;
        filev[0] = NULL;
        filev[1] = NULL;
        filev[2] = NULL;
        return;
      }
      filev[0] = args[i + 1];
      if (first_red == -1)
        first_red = i;
    } else if (strcmp(args[i], ">") == 0) {
      if (args[i + 1] == NULL) {
        redireccion_invalida = 1;
        filev[0] = NULL;
        filev[1] = NULL;
        filev[2] = NULL;
        return;
      }
      filev[1] = args[i + 1];
      if (first_red == -1)
        first_red = i;
    } else if (strcmp(args[i], "!>") == 0) {
      if (args[i + 1] == NULL) {
        redireccion_invalida = 1;
        filev[0] = NULL;
        filev[1] = NULL;
        filev[2] = NULL;
        return;
      }
      filev[2] = args[i + 1];
      if (first_red == -1)
        first_red = i;
    }
  }

  // Desde la primera redirección, lo que siga deja de formar parte de argv.
  if (first_red != -1)
    for (i = first_red; args[i] != NULL; i++) {
      args[i] = NULL;
    }
}

/**
 * This function processes the input command line and returns in global
 * variables: argvv -- command an args as argv filev -- files for redirections.
 * NULL value means no redirection. background -- 0 means foreground; 1
 * background.
 */
int procesar_linea(char *linea) {

  if (pipes_mal_formados(linea) == 1) {
    errno = EINVAL;
    perror("command");
    return -3;
  }

  char *comandos[max_commands];
  int num_comandos = tokenizar_linea(linea, "|", comandos, max_commands);
  background = 0;

  if (num_comandos <= 0) {
    errno = EINVAL;
    perror("command");
    return -3;
  }

  if (num_comandos > 3) {
    fprintf(stderr, "[ERROR] Máximo 3 comandos por tubería\n");
    return -2;
  }

  // Check if background is indicated at end of command line
  background = detectar_background(comandos[num_comandos - 1]);

  filev[0] = NULL;
  filev[1] = NULL;
  filev[2] = NULL;
  redireccion_invalida = 0;

  // Finish processing
  for (int i = 0; i < num_comandos; i++) {
    (void)tokenizar_argumentos(comandos[i], argvv_comandos[i], max_args);

    if (argvv_comandos[i][0] == NULL) {
      errno = EINVAL;
      perror("command");
      return -3;
    }

    for (int j = 0; argvv_comandos[i][j] != NULL; j++) {
      if (strcmp(argvv_comandos[i][j], "<") == 0 && i != 0) {
        errno = EINVAL;
        perror("redirection");
        return -3;
      }
      if ((strcmp(argvv_comandos[i][j], ">") == 0 ||
           strcmp(argvv_comandos[i][j], "!>") == 0) &&
          i != num_comandos - 1) {
        errno = EINVAL;
        perror("redirection");
        return -3;
      }
    }

    procesar_redirecciones(argvv_comandos[i]);

    if (redireccion_invalida == 1) {
      errno = EINVAL;
      perror("redirection");
      return -3;
    }
  }

  for (int i = 0; i < max_args; i++) {
    argvv[i] = argvv_comandos[0][i];
  }

  return num_comandos;
}

int main(int argc, char *argv[]) {
  int fd = -1;
  char linea[max_line];
  int pos = 0;
  int first_line = 1;
  ssize_t nread = 0;
  char c = '\0';

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <fichero_de_comandos>\n", argv[0]);
    return -1;
  }

  fd = open(argv[1], O_RDONLY);
  if (fd < 0) {
    perror("open");
    return -1;
  }

  while ((nread = read(fd, &c, 1)) > 0) {
    if (c == '\n') {
      linea[pos] = '\0';

      if (first_line) {
        if (strcmp(linea, "## Uc3mshell P2") != 0) {
          close(fd);
          errno = EINVAL;
          perror("script header");
          return -1;
        }
        first_line = 0;
      } else if (!linea_ignorable(linea)) {
        int n_commands = procesar_linea(linea);
        int resultado_interno = 0;

        recolectar_zombies();

        if (n_commands == -2 || n_commands == -3) {
          continue;
        }

        if (n_commands == 1) {
          resultado_interno = manejar_comando_interno(argvv_comandos[0], fd);

          if (resultado_interno < 0) {
            return -1;
          }
          if (resultado_interno == 0) {
            if (ejecutar_comando_simple(argvv_comandos[0]) < 0) {
              close(fd);
              return -1;
            }
          }
        } else {
          if (ejecutar_secuencia_pipes(n_commands) < 0) {
            close(fd);
            return -1;
          }
        }
      }

      pos = 0;
    } else if (pos < max_line - 1) {
      linea[pos++] = c;
    }
  }

  if (nread < 0) {
    perror("read");
    close(fd);
    return -1;
  }

  if (pos > 0) {
    linea[pos] = '\0';
    if (first_line) {
      if (strcmp(linea, "## Uc3mshell P2") != 0) {
        close(fd);
        errno = EINVAL;
        perror("script header");
        return -1;
      }
      first_line = 0;
    } else if (!linea_ignorable(linea)) {
      int n_commands = procesar_linea(linea);
      int resultado_interno = 0;

      recolectar_zombies();

      if (n_commands == -2 || n_commands == -3) {
        goto fin_ejecucion;
      }

      if (n_commands == 1) {
        resultado_interno = manejar_comando_interno(argvv_comandos[0], fd);

        if (resultado_interno < 0) {
          return -1;
        }
        if (resultado_interno == 0) {
          if (ejecutar_comando_simple(argvv_comandos[0]) < 0) {
            close(fd);
            return -1;
          }
        }
      } else {
        if (ejecutar_secuencia_pipes(n_commands) < 0) {
          close(fd);
          return -1;
        }
      }
    }
  }

fin_ejecucion:
  recolectar_zombies();

  if (first_line) {
    errno = EINVAL;
    perror("script header");
    close(fd);
    return -1;
  }

  if (close(fd) < 0) {
    perror("close");
    return -1;
  }

  return 0;
}
