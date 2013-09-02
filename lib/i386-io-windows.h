/*
 *	The PCI Library -- Access to i386 I/O ports on Windows
 *
 *  	Copyright (c) 2004 	Alexander Stock <stock.alexander@gmx.de>
 *  	Copyright (c) 2006 	Martin Mares <mj@ucw.cz> 
 *	Copyright (c) 2011-2012 Jernej Simončič <jernej+s-pciutils@eternallybored.org>
 *	Copyright (c) 2013      Samuel Pitoiset   <samuel.pitoiset@gmail.com>
 *
 *	Can be freely distributed and used under the terms of the GNU GPL.
 *
 * Changelog:
 * 2013-08-20: remove useless support of DirectIO
 * 2012-02-19: added back support for WinIo when DirectIO isn't available
 * 2011-06-20: original release for DirectIO library
 */

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>

typedef BOOL (WINAPI *SETDLLDIRECTORY)(LPCSTR);

#define LIB_DIRECTIO 1
#define LIB_WINIO 2
int lib_used;

/* WinIo declarations */
typedef BYTE bool;

typedef bool (_stdcall* INITIALIZEWINIO)(void);
typedef void (_stdcall* SHUTDOWNWINIO)(void);
typedef bool (_stdcall* GETPORTVAL)(WORD,PDWORD,BYTE);
typedef bool (_stdcall* SETPORTVAL)(WORD,DWORD,BYTE);
typedef bool (_stdcall* GETPHYSLONG)(PBYTE,PDWORD);
typedef bool (_stdcall* SETPHYSLONG)(PBYTE,DWORD);

SHUTDOWNWINIO ShutdownWinIo;
GETPORTVAL GetPortVal;
SETPORTVAL SetPortVal;
INITIALIZEWINIO InitializeWinIo;
GETPHYSLONG GetPhysLong;
SETPHYSLONG SetPhysLong;

static int
intel_setup_io(struct pci_access *a)
{
  HMODULE lib;

  SETDLLDIRECTORY fnSetDllDirectory;

  /* remove current directory from DLL search path */
  fnSetDllDirectory = GetProcAddress(GetModuleHandle("kernel32"), "SetDllDirectoryA");
  if (fnSetDllDirectory)
    fnSetDllDirectory("");

  if ((GetVersion() & 0x80000000) == 0) {
    /* running on NT, try WinIo version 3 (32 or 64 bits) */
#ifdef WIN64
    lib = LoadLibrary("WinIo64.dll");
#else
    lib = LoadLibrary("WinIo32.dll");
#endif
  }

  if (!lib) {
    a->warning("i386-io-windows: Failed to load WinIo library.\n");
    return 0;
  }

#define GETPROC(n, d) 							\
  n = (d) GetProcAddress(lib, #n); 					\
  if (!n) { 								\
    a->warning("i386-io-windows: Failed to load " #n " function.\n"); 	\
    return 0; 								\
  }

  GETPROC(InitializeWinIo, INITIALIZEWINIO);
  GETPROC(ShutdownWinIo, SHUTDOWNWINIO);
  GETPROC(GetPortVal, GETPORTVAL);
  GETPROC(SetPortVal, SETPORTVAL);
  GETPROC(GetPhysLong, GETPHYSLONG);
  GETPROC(SetPhysLong, SETPHYSLONG);

#undef GETPROC

  if (!InitializeWinIo()) {
      a->warning("i386-io-windows: Failed to initialize WinIo.\n");
      return 0;
  }

  return 1;
}

static inline int
intel_cleanup_io(struct pci_access *a UNUSED)
{
  ShutdownWinIo();
  return 1;
}

static inline u8
inb(u16 port)
{
  DWORD pv;

  if (GetPortVal(port, &pv, 1))
    return (u8)pv;
  return 0;
}

static inline u16
inw(u16 port)
{
  DWORD pv;
  
  if (GetPortVal(port, &pv, 2))
    return (u16)pv;
  return 0;
}

static inline u32
inl(u16 port)
{
  DWORD pv;

  if (GetPortVal(port, &pv, 4))
    return (u32)pv;
  return 0;
}

static inline void
outb(u8 value, u16 port)
{
  SetPortVal(port, value, 1);
}

static inline void
outw(u16 value, u16 port)
{
  SetPortVal(port, value, 2);
}

static inline void
outl(u32 value, u16 port)
{
  SetPortVal(port, value, 4);
}

static uint32_t
nva_rd32(int32_t reg)
{
  DWORD value;

  /* XXX: This read at the first region of my GPU (FIXME)! */
  if (GetPhysLong(0xce000000 + reg, &value))
    return (uint32_t)value;
  return 0;
}

static void
nva_wr32(int32_t reg, uint32_t value)
{
  /* XXX: This write at the first region of my GPU (FIXME)! */
  SetPhysLong(0xce000000 + reg, value);
}
