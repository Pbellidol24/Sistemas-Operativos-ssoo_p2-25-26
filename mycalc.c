#include "mycalc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int mycalc(int argc, char **argv) {
  long op1 = 0;
  long op2 = 0;
  long resultado = 0;
  char *endptr1 = NULL;
  char *endptr2 = NULL;

  if (argc != 4) {
    fprintf(stderr, "Usage: mycalc <num1> < + | - | x | / > <num2>\n");
    return -1;
  }

  op1 = strtol(argv[1], &endptr1, 10);
  op2 = strtol(argv[3], &endptr2, 10);

  if (endptr1 == argv[1] || *endptr1 != '\0' || endptr2 == argv[3] ||
      *endptr2 != '\0') {
    fprintf(stderr, "Usage: mycalc <num1> < + | - | x | / > <num2>\n");
    return -1;
  }

  if (strcmp(argv[2], "+") == 0) {
    resultado = op1 + op2;
  } else if (strcmp(argv[2], "-") == 0) {
    resultado = op1 - op2;
  } else if (strcmp(argv[2], "x") == 0) {
    resultado = op1 * op2;
  } else if (strcmp(argv[2], "/") == 0) {
    if (op2 == 0) {
      fprintf(stderr, "[ERROR] División por cero\n");
      return -1;
    }
    resultado = op1 / op2;
  } else {
    fprintf(stderr, "Usage: mycalc <num1> < + | - | x | / > <num2>\n");
    return -1;
  }

  printf("Operación: %ld %s %ld = %ld\n", op1, argv[2], op2, resultado);

  return 0;
}
