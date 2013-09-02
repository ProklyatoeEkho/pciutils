#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#include "lspci.h"
#include "lib/i386-io-windows.h"

const char program_name[] = "nvapeek";

int
main(int argc, char **argv)
{
  struct pci_filter filter;
  struct pci_access *pacc;
  int32_t a, offset = 0x4;
  uint32_t value;
  int32_t i;

  if (argc < 2) {
    fprintf(stderr, "%s: No address specified.\n", program_name);
    return 1;
  }

  sscanf(argv[1], "%x", &a);

  if (argc > 2)
    sscanf(argv[2], "%x", &offset);

  pacc = pci_alloc();
  pacc->error = die;
  pci_filter_init(pacc, &filter);
  pci_init(pacc);

  for (i = a; i < a + offset; i += 0x4) {
    value = nva_rd32(i);
    printf("%x: ", i);
    if (value == 0)
      printf("...\n");
    else
      printf("%08x\n", value);
  }

  pci_cleanup(pacc);

  return 0;
}
