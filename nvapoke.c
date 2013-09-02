#include <stdio.h>
#include <unistd.h>

#include "lspci.h"
#include "lib/i386-io-windows.h"

const char program_name[] = "nvapoke";

int
main(int argc, char **argv)
{
  struct pci_filter filter;
  struct pci_access *pacc;
  uint32_t address, value;

  if (argc < 3) {
    fprintf(stderr, "Usage: %s <address> <value>.\n", argv[0]);
    return 1;
  }

  sscanf(argv[1], "%x", &address);
  sscanf(argv[2], "%x", &value);

  pacc = pci_alloc();
  pacc->error = die;
  pci_filter_init(pacc, &filter);
  pci_init(pacc);

  nva_wr32(address, value);

  pci_cleanup(pacc);

  return 0;
}
