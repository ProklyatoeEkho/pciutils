#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#include "lspci.h"
#include "lib/i386-io-windows.h"

const char program_name[] = "nva_counters_config";

struct reg32 {
  uint32_t address;
  uint32_t value;
};

struct pcounter {

  struct {
    struct {
      struct reg32 pre;
      struct reg32 start;
      struct reg32 event;
      struct reg32 stop;
      struct reg32 status;
      struct reg32 spec;
    } src;
  } set[8];

  struct {
    struct reg32 _pre[8];

    uint32_t pre[8];
    uint32_t start[8];
    uint32_t event[8];
    uint32_t stop[8];
    uint32_t status[8];
    uint32_t spec[8];
  } src;

  struct {
    uint32_t pre[8];
    uint32_t start[8];
    uint32_t event[8];
    uint32_t stop[8];
    uint32_t setflag[8];
    uint32_t clrflag[8];
  } op;

  struct {
    uint32_t pre[8];
    uint32_t start[8];
    uint32_t event[8];
    uint32_t stop[8];
    uint32_t cycles[8];
    uint32_t cycles_alt[8];
  } ctr;

  uint32_t ctrl[8];
  uint32_t quad_ack_trigger[8];

  uint32_t gctrl;
};

struct pgraph {
  uint32_t disable;
  uint32_t status;

  struct {
    uint32_t control;  
  } fifo;

  struct {
    struct {
      uint32_t code_config_0;
      uint32_t code_config_1;
      uint32_t pm_sig;
      uint32_t error_code;
      uint32_t unk18;
      uint32_t unk20;
      uint32_t pc_current;
      uint32_t pm_counter[4];
    } mp[2];

    struct {
      uint32_t trap_en;
      uint32_t pm_group_sel;
      uint32_t unk34;
      uint32_t pm_out_enable;
    } mpc;
  } tp[1];

  uint32_t pfb_unk334;
};

#define LOOKUP_PATH "/cygdrive/c/Users/hakzsam/Programming/envytools/build/rnn/lookup.exe"

static int
lookup(const char *chipset, uint32_t address, uint32_t value)
{
    char cmd[1024], buf[1024];
    FILE *f;

    sprintf(cmd, "%s -a %s %x %x\n", LOOKUP_PATH, chipset, address, value);

    if (!(f = popen(cmd, "r"))) {
        perror("popen");
        return -1;
    }

    while (fgets(buf, sizeof(buf), f) != NULL)
        fprintf(stderr, "%s", buf);

    if (pclose(f) < 0) {
        perror("pclose");
        return -1;
    }

    return 0;
}

static uint32_t
read_current_channel(void)
{
  return nva_rd32(0x40032c); // PGRAPH.CTXCTL_CUR
}

static struct pcounter *
nv40_alloc_pcounter(void)
{
  struct pcounter *p;
  int i;

  if (!(p = malloc(sizeof(*p))))
    return NULL;

  for (i = 0; i < 8; i++) {
    p->set[i].src.pre.address    = 0xa400 + (i * 0x4);
    p->set[i].src.start.address  = 0xa440 + (i * 0x4);
    p->set[i].src.event.address  = 0xa480 + (i * 0x4);
    p->set[i].src.stop.address   = 0xa4c0 + (i * 0x4);
    p->set[i].src.status.address = 0xa540 + (i * 0x4);
    p->set[i].src.spec.address   = 0xa560 + (i * 0x4);
  }

  return p;
}

static void
__nv40_dump_pcounter(struct pcounter *p)
{
  int i;

  for (i = 0; i < 8; i++) {
    p->set[i].src.pre.value    = nva_rd32(p->set[i].src.pre.address);
    p->set[i].src.start.value  = nva_rd32(p->set[i].src.start.address);
    p->set[i].src.event.value  = nva_rd32(p->set[i].src.event.address);
    p->set[i].src.stop.value   = nva_rd32(p->set[i].src.stop.address);
    p->set[i].src.status.value = nva_rd32(p->set[i].src.status.address);
    p->set[i].src.spec.value   = nva_rd32(p->set[i].src.spec.address);
  }
}

static struct pcounter *
nv40_dump_pcounter(void)
{
  struct pcounter *p;
  int i;

  if (!(p = malloc(sizeof(*p))))
    return NULL;

  for (i = 0; i < 8; i++) {
    p->src.pre[i]           = nva_rd32(0xa400 + (i * 0x4));
    p->src.start[i]         = nva_rd32(0xa440 + (i * 0x4));
    p->src.event[i]         = nva_rd32(0xa480 + (i * 0x4));
    p->src.stop[i]          = nva_rd32(0xa4c0 + (i * 0x4));
    p->src.status[i]        = nva_rd32(0xa540 + (i * 0x4));
    p->src.spec[i]          = nva_rd32(0xa560 + (i * 0x4));

    p->op.pre[i]            = nva_rd32(0xa420 + (i * 0x4));
    p->op.start[i]          = nva_rd32(0xa460 + (i * 0x4));
    p->op.event[i]          = nva_rd32(0xa4a0 + (i * 0x4));
    p->op.stop[i]           = nva_rd32(0xa4e0 + (i * 0x4));
    p->op.setflag[i]        = nva_rd32(0xa500 + (i * 0x4));
    p->op.clrflag[i]        = nva_rd32(0xa520 + (i * 0x4));

    p->ctr.pre[i]           = nva_rd32(0xa700 + (i * 0x4));
    p->ctr.start[i]         = nva_rd32(0xa6c0 + (i * 0x4));
    p->ctr.event[i]         = nva_rd32(0xa680 + (i * 0x4));
    p->ctr.stop[i]          = nva_rd32(0xa740 + (i * 0x4));
    p->ctr.cycles[i]        = nva_rd32(0xa600 + (i * 0x4));
    p->ctr.cycles_alt[i]    = nva_rd32(0xa640 + (i * 0x4));

    p->ctrl[i]              = nva_rd32(0xa7c0 + (i * 0x4));
    p->quad_ack_trigger[i]  = nva_rd32(0xa7e0 + (i * 0x4));
  }

  p->gctrl = nva_rd32(0xa7a8);

  return p;
}

static void
__nv40_print_pcounter(struct pcounter *p)
{
  int i;

#define p(r) 						      			 \
  fprintf(stderr, "addr=%x, val=%x : ", p->set[i].r.address, p->set[i].r.value); \
  lookup("NV86", p->set[i].r.address, p->set[i].r.value);

  for (i = 0; i < 8; i++) {
    p(src.pre);
    fprintf(stderr, "aadr=%x, val=%x : ", p->set[i].src.pre.address, p->set[i].src.pre.value);
    lookup("NV86", p->set[i].src.pre.address,    p->set[i].src.pre.value);

    fprintf(stderr, "%x: ", p->set[i].src.start.address);
    lookup("NV86", p->set[i].src.start.address,  p->set[i].src.start.value);

    fprintf(stderr, "%x: ", p->set[i].src.event.address);
    lookup("NV86", p->set[i].src.event.address,  p->set[i].src.event.value);

    fprintf(stderr, "%x: ", p->set[i].src.stop.address);
    lookup("NV86", p->set[i].src.stop.address,   p->set[i].src.stop.value);

    fprintf(stderr, "%x: ", p->set[i].src.status.address);
    lookup("NV86", p->set[i].src.status.address, p->set[i].src.status.value);

    fprintf(stderr, "%x: ", p->set[i].src.spec.address);
    lookup("NV86", p->set[i].src.spec.address,   p->set[i].src.spec.value);

    fprintf(stderr, "\n");
  }
}

static void
nv40_print_pcounter(struct pcounter *p, int with_counters)
{
  int i;

  for (i = 0; i < 8; i++) {
    fprintf(stderr, "\nSet #%d :\n", i);
    fprintf(stderr, "PRE_SRC[%x]           = %x\n", i, p->src.pre[i]);
    fprintf(stderr, "START_SRC[%x]         = %x\n", i, p->src.start[i]);
    fprintf(stderr, "EVENT_SRC[%x]         = %x\n", i, p->src.event[i]);
    fprintf(stderr, "STOP_SRC[%x]          = %x\n", i, p->src.stop[i]);
    fprintf(stderr, "STATUS_SRC[%x]        = %x\n", i, p->src.status[i]);
    fprintf(stderr, "SPEC_SRC[%x]          = %x\n", i, p->src.spec[i]);

    fprintf(stderr, "PRE_OP[%x]            = %x\n", i, p->op.pre[i]);
    fprintf(stderr, "START_OP[%x]          = %x\n", i, p->op.start[i]);
    fprintf(stderr, "EVENT_OP[%x]          = %x\n", i, p->op.event[i]);
    fprintf(stderr, "STOP_OP[%x]           = %x\n", i, p->op.stop[i]);
    fprintf(stderr, "SETFLAG_OP[%x]        = %x\n", i, p->op.setflag[i]);
    fprintf(stderr, "CLRFLAG_OP[%x]        = %x\n", i, p->op.clrflag[i]);

    if (with_counters) {
      fprintf(stderr, "PRE_CTR[%x]           = %x\n", i, p->ctr.pre[i]);
      fprintf(stderr, "START_CTR[%x]         = %x\n", i, p->ctr.start[i]);
      fprintf(stderr, "EVENT_CTR[%x]         = %x\n", i, p->ctr.event[i]);
      fprintf(stderr, "STOP_CTR[%x]          = %x\n", i, p->ctr.stop[i]);
      fprintf(stderr, "CYCLES_CTR[%x]        = %x\n", i, p->ctr.cycles[i]);
      fprintf(stderr, "CYCLES_ALT_CTR[%x]    = %x\n", i, p->ctr.cycles_alt[i]);
    }

    fprintf(stderr, "CTRL[%x]              = %x\n", i, p->ctrl[i]);
    fprintf(stderr, "QUACK_ACK_TRIGGER[%x] = %x\n", i, p->quad_ack_trigger[i]);
  }

  fprintf(stderr, "CGTRL = %x\n", p->gctrl);
}

static void
nv40_diff_pcounter(struct pcounter *old, struct pcounter *new, int with_counters)
{
  int i;

  for (i = 0; i < 8; i++) {
    fprintf(stderr, "\nSet #%d :\n", i);

    if (old->src.pre[i] != new->src.pre[i])
      fprintf(stderr, "PRE_SRC[%x] %x => %x\n", i, old->src.pre[i], new->src.pre[i]);
    if (old->src.start[i] != new->src.start[i])
      fprintf(stderr, "START_SRC[%x] %x => %x\n", i, old->src.start[i], new->src.start[i]);
    if (old->src.event[i] != new->src.event[i])
      fprintf(stderr, "EVENT_SRC[%x] %x => %x\n", i, old->src.event[i], new->src.event[i]);
    if (old->src.stop[i] != new->src.stop[i])
      fprintf(stderr, "STOP_SRC[%x] %x => %x\n", i, old->src.stop[i], new->src.stop[i]);

    if (old->op.pre[i] != new->op.pre[i])
      fprintf(stderr, "PRE_OP[%x] %x => %x\n", i, old->op.pre[i], new->op.pre[i]);
    if (old->op.start[i] != new->op.start[i])
      fprintf(stderr, "START_OP[%x] %x => %x\n", i, old->op.start[i], new->op.start[i]);
    if (old->op.event[i] != new->op.event[i])
      fprintf(stderr, "EVENT_OP[%x] %x => %x\n", i, old->op.event[i], new->op.event[i]);
    if (old->op.stop[i] != new->op.stop[i])
      fprintf(stderr, "STOP_OP[%x] %x => %x\n", i, old->op.stop[i], new->op.stop[i]);

    if (with_counters) {
      if (old->ctr.pre[i] != new->ctr.pre[i])
        fprintf(stderr, "PRE_CTR[%x] %x => %x\n", i, old->ctr.pre[i], new->ctr.pre[i]);
      if (old->ctr.start[i] != new->ctr.start[i])
        fprintf(stderr, "START_CTR[%x] %x => %x\n", i, old->ctr.start[i], new->ctr.start[i]);
      if (old->ctr.event[i] != new->ctr.event[i])
        fprintf(stderr, "EVENT_CTR[%x] %x => 0x%x (%d)\n", i, old->ctr.event[i], new->ctr.event[i], new->ctr.event[i]);
      if (old->ctr.stop[i] != new->ctr.stop[i])
        fprintf(stderr, "STOP_CTR[%x] %x => %x\n", i, old->ctr.stop[i], new->ctr.stop[i]);
      if (old->ctr.cycles[i] != new->ctr.cycles[i])
        fprintf(stderr, "CYCLES_CTR[%x] %x => 0x%x (%d)\n", i, old->ctr.cycles[i], new->ctr.cycles[i], new->ctr.cycles[i]);
    }

    if (i == 1) {
      double d = (float)new->ctr.event[i] * 100.0 / new->ctr.cycles[i];
      printf("%6.2f\n", d);
    }
  }
}

static struct pgraph *
nv50_dump_pgraph(void)
{
  struct pgraph *p;
  int i;

  if (!(p = malloc(sizeof(*p))))
    return NULL;

  p->disable = nva_rd32(0x400040);
  p->status  = nva_rd32(0x400700);

  p->fifo.control = nva_rd32(0x400800);

  for (i = 0; i < 2; i++) {
    p->tp[0].mp[i].code_config_0 = nva_rd32(0x408200 + (i * 0x80) + 0x00);
    p->tp[0].mp[i].code_config_1 = nva_rd32(0x408200 + (i * 0x80) + 0x08);
    p->tp[0].mp[i].pm_sig        = nva_rd32(0x408200 + (i * 0x80) + 0x10);
    p->tp[0].mp[i].error_code    = nva_rd32(0x408200 + (i * 0x80) + 0x14);
    p->tp[0].mp[i].unk18         = nva_rd32(0x408200 + (i * 0x80) + 0x18);
    p->tp[0].mp[i].unk20         = nva_rd32(0x408200 + (i * 0x80) + 0x20);
    p->tp[0].mp[i].pc_current    = nva_rd32(0x408200 + (i * 0x80) + 0x24);
    p->tp[0].mp[i].pm_counter[0] = nva_rd32(0x408200 + (i * 0x80) + 0x28);
    p->tp[0].mp[i].pm_counter[1] = nva_rd32(0x408200 + (i * 0x80) + 0x30);
    p->tp[0].mp[i].pm_counter[2] = nva_rd32(0x408200 + (i * 0x80) + 0x38);
    p->tp[0].mp[i].pm_counter[3] = nva_rd32(0x408200 + (i * 0x80) + 0x40);
  }

  p->tp[0].mpc.trap_en       = nva_rd32(0x408318);
  p->tp[0].mpc.pm_group_sel  = nva_rd32(0x408330);
  p->tp[0].mpc.unk34         = nva_rd32(0x408334);
  p->tp[0].mpc.pm_out_enable = nva_rd32(0x40833c);

  p->pfb_unk334              = nva_rd32(0x100334);

  return p;
}

static void
nv50_print_pgraph(struct pgraph *p)
{
  int i, j;

  fprintf(stderr, "PGRAPH.DISABLE                     \t = %x\n", p->disable);
  fprintf(stderr, "PGRAPH.STATUS                      \t = %x\n", p->status);

  fprintf(stderr, "PGRAPH.FIFO.CONTROL                \t = %x\n", p->fifo.control);

  for (i = 0; i < 2; i++) {
    fprintf(stderr, "PGRAPH.TP[0].MP[%d].CODE_CONFIG_O\t = %x\n", i, p->tp[0].mp[i].code_config_0);
    fprintf(stderr, "PGRAPH.TP[0].MP[%d].CODE_CONFIG_1\t = %x\n", i, p->tp[0].mp[i].code_config_1);
    fprintf(stderr, "PGRAPH.TP[0].MP[%d].PM_SIG       \t = %x\n", i, p->tp[0].mp[i].pm_sig);
    fprintf(stderr, "PGRAPH.TP[0].MP[%d].ERROR_CODE   \t = %x\n", i, p->tp[0].mp[i].error_code);
    fprintf(stderr, "PGRAPH.TP[0].MP[%d].UNK18        \t = %x\n", i, p->tp[0].mp[i].unk18);
    fprintf(stderr, "PGRAPH.TP[0].MP[%d].UNK20        \t = %x\n", i, p->tp[0].mp[i].unk20);
    fprintf(stderr, "PGRAPH.TP[0].MP[%d].PC_CURRENT   \t = %x\n", i, p->tp[0].mp[i].pc_current);

    for (j = 0; j < 4; j++)
      fprintf(stderr, "PGRAPH.TP[0].MP[%d].PM_COUNTER[%d]\t = %x\n", i, j, p->tp[0].mp[i].pm_counter[j]);
  }

  fprintf(stderr, "PGRAPH.TP[0].MPC.TRAP_EN           \t = %x\n", p->tp[0].mpc.trap_en);
  fprintf(stderr, "PGRAPH.TP[0].MPC.PM_GROUP_SEL      \t = %x\n", p->tp[0].mpc.pm_group_sel);
  fprintf(stderr, "PGRAPH.TP[0].MPC.UNK34             \t = %x\n", p->tp[0].mpc.unk34);
  fprintf(stderr, "PGRAPH.TP[0].MPC.PM_OUT_ENABLE     \t = %x\n", p->tp[0].mpc.pm_out_enable);

  fprintf(stderr, "PFB+0x334                          \t = %x\n", p->pfb_unk334);
}

int
main(void)
{
  struct pcounter *old_pc, *new_pc;
  struct pgraph *old_pg, *new_pg;
  struct pci_filter filter;
  struct pci_access *pacc;
  int dump_id = 0;

  pacc = pci_alloc();
  pacc->error = die;
  pci_filter_init(pacc, &filter);
  pci_init(pacc);

  old_pc = NULL;
  old_pg = NULL;
  while (1) {
    uint32_t channel;

    channel = read_current_channel();

    new_pg = nv50_dump_pgraph();
    new_pc = nv40_dump_pcounter();

    if (channel != read_current_channel()) {
      fprintf(stderr, "The channel has changed during the dump. Abort!\n"); 
    } else if (old_pg && old_pc) {
      fprintf(stderr, "============================ Dump #%d =========================\n", dump_id);
      fprintf(stderr, "Channel: %x\n", channel);
      nv40_diff_pcounter(old_pc, new_pc, 1);
      nv50_print_pgraph(new_pg);
      //nv40_print_pcounter(new_pc, 1);

      fprintf(stderr, "===============================================================\n");
      dump_id++;
    }

    free(old_pc);
    free(old_pg);

    old_pc = new_pc;
    old_pg = new_pg;

    sleep(1);
  }
  
  pci_cleanup(pacc);

  return 0;
}
