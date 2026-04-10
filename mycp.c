#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
  int fd_src = -1;
  int fd_dst = -1;
  ssize_t nread = 0;
  char buffer[4096];

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <source_file> <destination_file>\n", argv[0]);
    return -1;
  }

  fd_src = open(argv[1], O_RDONLY);
  if (fd_src < 0) {
    perror("open");
    return -1;
  }

  fd_dst = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd_dst < 0) {
    perror("open");
    if (close(fd_src) < 0) {
      perror("close");
    }
    return -1;
  }

  while ((nread = read(fd_src, buffer, sizeof(buffer))) > 0) {
    ssize_t total_written = 0;

    while (total_written < nread) {
      ssize_t nwritten = write(fd_dst, buffer + total_written,
                               (size_t)(nread - total_written));
      if (nwritten < 0) {
        perror("write");
        if (close(fd_src) < 0) {
          perror("close");
        }
        if (close(fd_dst) < 0) {
          perror("close");
        }
        return -1;
      }
      total_written += nwritten;
    }
  }

  if (nread < 0) {
    perror("read");
    if (close(fd_src) < 0) {
      perror("close");
    }
    if (close(fd_dst) < 0) {
      perror("close");
    }
    return -1;
  }

  if (close(fd_src) < 0) {
    perror("close");
    if (close(fd_dst) < 0) {
      perror("close");
    }
    return -1;
  }

  if (close(fd_dst) < 0) {
    perror("close");
    return -1;
  }

  return 0;
}
