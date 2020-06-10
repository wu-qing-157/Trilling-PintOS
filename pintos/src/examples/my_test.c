#include <stdio.h>
#include <syscall.h>

int
main (int argc, char **argv)
{
  create (NULL, 0);
  return EXIT_SUCCESS;
}

