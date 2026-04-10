# Checklist de compilacion y pruebas (Linux/Guernika)

## 1) Compilacion

1. make clean
2. make
3. make uc3mshell
4. make mycp

Si falla, guarda el error completo y revisa el fichero/lina indicada.

## 2) Pruebas basicas

1. ./uc3mshell test_script_bad_header.txt
Esperado: error de cabecera y salida -1.

2. ./uc3mshell test_script_ok.txt
Esperado: ejecuta comandos, mycalc imprime Operacion, mycp copia fichero, exit imprime Goodbye 0.

3. ./uc3mshell test_script_exit_errors.txt
Esperado: muestra los dos mensajes de error de exit y finalmente Goodbye 3.

## 3) Pruebas de borde

1. ./uc3mshell test_script_redirection_errors.txt
Esperado: perror por redireccion invalida, se omite esa linea y continua con la siguiente.

2. ./uc3mshell test_script_pipe_limit.txt
Esperado: error por superar 3 comandos en tuberia, pero el script continua y ejecuta la linea siguiente.

## 4) Prueba de zombies

1. Crear script con varios sleep en background y un comando foreground al final.
2. Ejecutar ./uc3mshell script_bg.txt
3. Comprobar despues con ps que no queden procesos zombie del shell.

## 5) Prueba de ficheros (mycp y redirecciones)

1. ls -l test_input.txt test_output.txt test_copy.txt test_pipe_out.txt test_err.txt
2. cat test_output.txt
3. cat test_copy.txt

## 6) Entrega

Asegura incluir solo:
- uc3mshell.c
- mycp.c
- autores.txt
- mycalc.c
- mycalc.h
- Makefile
