/*
  Hatari - debugcpu.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  debugcpu.c - function needed for the CPU debugging tasks like memory
  and register dumps.
*/
const char DebugCpu_fileid[] = "Hatari debugcpu.c : " __DATE__ " " __TIME__;

#include <stdio.h>

#include "config.h"

#include "main.h"
#include "breakcond.h"
#include "debugui.h"
#include "debug_priv.h"
#include "debugcpu.h"
#include "evaluate.h"
#include "hatari-glue.h"
#include "log.h"
#include "m68000.h"
#include "memorySnapShot.h"
#include "stMemory.h"
#include "str.h"
#include "symbols.h"

#define MEMDUMP_COLS   16      /* memdump, number of bytes per row */
#define MEMDUMP_ROWS   4       /* memdump, number of rows */
#define NON_PRINT_CHAR '.'     /* character to display for non-printables */
#define DISASM_INSTS   5       /* disasm - number of instructions */

static Uint32 disasm_addr;     /* disasm address */
static Uint32 memdump_addr;    /* memdump address */

static int nCpuActiveCBs = 0;  /* Amount of active conditional breakpoints */
static int nCpuSteps = 0;      /* Amount of steps for CPU single-stepping */


/**
 * Save/Restore snapshot of CPU debugging session variables
 */
void DebugCpu_MemorySnapShot_Capture(bool bSave)
{
	MemorySnapShot_Store(&disasm_addr, sizeof(disasm_addr));
	MemorySnapShot_Store(&memdump_addr, sizeof(memdump_addr));
	
	MemorySnapShot_Store(&nCpuActiveCBs, sizeof(nCpuActiveCBs));
}


/**
 * Load a binary file to a memory address.
 */
static int DebugCpu_LoadBin(int nArgc, char *psArgs[])
{
	FILE *fp;
	unsigned char c;
	Uint32 address;
	int i=0;

	if (nArgc < 3)
	{
		DebugUI_PrintCmdHelp(psArgs[0]);
		return DEBUGGER_CMDDONE;
	}

	if (!Eval_Number(psArgs[2], &address))
	{
		fprintf(stderr, "Invalid address!\n");
		return DEBUGGER_CMDDONE;
	}
	address &= 0x00FFFFFF;

	if ((fp = fopen(psArgs[1], "rb")) == NULL)
	{
		fprintf(stderr, "Cannot open file '%s'!\n", psArgs[1]);
		return DEBUGGER_CMDDONE;
	}

	c = fgetc(fp);
	while (!feof(fp))
	{
		i++;
		STMemory_WriteByte(address++, c);
		c = fgetc(fp);
	}
	fprintf(stderr,"  Read 0x%x bytes.\n", i);
	fclose(fp);

	return DEBUGGER_CMDDONE;
}


/**
 * Dump memory from an address to a binary file.
 */
static int DebugCpu_SaveBin(int nArgc, char *psArgs[])
{
	FILE *fp;
	unsigned char c;
	Uint32 address;
	Uint32 bytes, i = 0;

	if (nArgc < 4)
	{
		DebugUI_PrintCmdHelp(psArgs[0]);
		return DEBUGGER_CMDDONE;
	}

	if (!Eval_Number(psArgs[2], &address))
	{
		fprintf(stderr, "  Invalid address!\n");
		return DEBUGGER_CMDDONE;
	}
	address &= 0x00FFFFFF;

	if (!Eval_Number(psArgs[3], &bytes))
	{
		fprintf(stderr, "  Invalid length!\n");
		return DEBUGGER_CMDDONE;
	}

	if ((fp = fopen(psArgs[1], "wb")) == NULL)
	{
		fprintf(stderr,"  Cannot open file '%s'!\n", psArgs[1]);
		return DEBUGGER_CMDDONE;
	}

	while (i < bytes)
	{
		c = STMemory_ReadByte(address++);
		fputc(c, fp);
		i++;
	}
	fclose(fp);
	fprintf(stderr, "  Wrote 0x%x bytes.\n", bytes);

	return DEBUGGER_CMDDONE;
}


/**
 * Check whether given address matches any CPU symbol, if yes,
 * show the symbol information.
 */
static void DebugCpu_ShowMatchedSymbol(Uint32 addr)
{
	const char *symbol = Symbols_GetByCpuAddress(addr);
	if (symbol)
		fprintf(debugOutput, "%s:\n", symbol);
}

/**
 * Dissassemble - arg = starting address, or PC.
 */
int DebugCpu_DisAsm(int nArgc, char *psArgs[])
{
	Uint32 disasm_upper = 0;
	int insts, max_insts;
	uaecptr nextpc;

	if (nArgc > 1)
	{
		switch (Eval_Range(psArgs[1], &disasm_addr, &disasm_upper))
		{
		case -1:
			/* invalid value(s) */
			return DEBUGGER_CMDDONE;
		case 0:
			/* single value */
			break;
		case 1:
			/* range */
			disasm_upper &= 0x00FFFFFF;
			break;
		}
	}
	else
	{
		/* continue */
		if(!disasm_addr)
			disasm_addr = M68000_GetPC();
	}
	disasm_addr &= 0x00FFFFFF;

	/* limit is topmost address or instruction count */
	if (disasm_upper)
	{
		max_insts = INT_MAX;
	}
	else
	{
		disasm_upper = 0x00FFFFFF;
		max_insts = DISASM_INSTS;
	}

	/* output a range */
	for (insts = 0; insts < max_insts && disasm_addr < disasm_upper; insts++)
	{
		DebugCpu_ShowMatchedSymbol(disasm_addr);
		m68k_disasm(debugOutput, (uaecptr)disasm_addr, &nextpc, 1);
		disasm_addr = nextpc;
	}
	fflush(debugOutput);

	return DEBUGGER_CMDCONT;
}


/**
 * Set address of the named register to given argument.
 * Return register size in bits or zero for uknown register name.
 * Handles D0-7 data and A0-7 address registers, but not PC & SR
 * registers as they need to be accessed using UAE accessors.
 */
int DebugCpu_GetRegisterAddress(const char *reg, Uint32 **addr)
{
	char r0, r1;
	if (!reg[0] || !reg[1] || reg[2])
		return 0;
	
	r0 = toupper(reg[0]);
	r1 = toupper(reg[1]);

	if (r0 == 'D')  /* Data regs? */
	{
		if (r1 >= '0' && r1 <= '7')
		{
			*addr = &(Regs[REG_D0 + r1 - '0']);
			return 32;
		}
		fprintf(stderr,"\tBad data register, valid values are 0-7\n");
		return 0;
	}
	if(r0 == 'A')  /* Address regs? */
	{
		if (r1 >= '0' && r1 <= '7')
		{
			*addr = &(Regs[REG_A0 + r1 - '0']);
			return 32;
		}
		fprintf(stderr,"\tBad address register, valid values are 0-7\n");
		return 0;
	}
	return 0;
}


/**
 * Dump or set CPU registers
 */
int DebugCpu_Register(int nArgc, char *psArgs[])
{
	char reg[3], *assign;
	Uint32 value;
	char *arg;

	/* If no parameter has been given, simply dump all registers */
	if (nArgc == 1)
	{
		uaecptr nextpc;
		/* use the UAE function instead */
		m68k_dumpstate(debugOutput, &nextpc);
		fflush(debugOutput);
		return DEBUGGER_CMDDONE;
	}

	arg = psArgs[1];

	assign = strchr(arg, '=');
	if (!assign)
	{
		goto error_msg;
	}

	*assign++ = '\0';
	if (!Eval_Number(Str_Trim(assign), &value))
	{
		goto error_msg;
	}

	arg = Str_Trim(arg);
	if (strlen(arg) != 2)
	{
		goto error_msg;
	}
	reg[0] = toupper(arg[0]);
	reg[1] = toupper(arg[1]);
	reg[2] = '\0';
	
	/* set SR and update conditional flags for the UAE CPU core. */
	if (reg[0] == 'S' && reg[1] == 'R')
	{
		M68000_SetSR(value);
	}
	else if (reg[0] == 'P' && reg[1] == 'C')   /* set PC? */
	{
		M68000_SetPC(value);
	}
	else
	{
		Uint32 *regaddr;
		/* check&set data and address registers */
		if (DebugCpu_GetRegisterAddress(reg, &regaddr))
		{
			*regaddr = value;
		}
		else
		{
			goto error_msg;
		}
	}
	return DEBUGGER_CMDDONE;

error_msg:
	fprintf(stderr,"\tError, usage: r or r xx=yyyy\n\tWhere: xx=A0-A7, D0-D7, PC or SR.\n");
	return DEBUGGER_CMDDONE;
}


/**
 * CPU wrapper for BreakAddr_Command/BreakPointCount.
 */
static int DebugCpu_BreakAddr(int nArgc, char *psArgs[])
{
	BreakAddr_Command(psArgs[1], false);
	nCpuActiveCBs = BreakCond_BreakPointCount(false);
	return DEBUGGER_CMDDONE;
}

/**
 * CPU wrapper for BreakCond_Command/BreakPointCount.
 */
static int DebugCpu_BreakCond(int nArgc, char *psArgs[])
{
	BreakCond_Command(psArgs[1], false);
	nCpuActiveCBs = BreakCond_BreakPointCount(false);
	return DEBUGGER_CMDDONE;
}


/**
 * Do a memory dump, args = starting address.
 */
int DebugCpu_MemDump(int nArgc, char *psArgs[])
{
	int i;
	char c;
	Uint32 memdump_upper = 0;

	if (nArgc > 1)
	{
		switch (Eval_Range(psArgs[1], &memdump_addr, &memdump_upper))
		{
		case -1:
			/* invalid value(s) */
			return DEBUGGER_CMDDONE;
		case 0:
			/* single value */
			break;
		case 1:
			/* range */
			break;
		}
	} /* continue */
	memdump_addr &= 0x00FFFFFF;

	if (!memdump_upper)
	{
		memdump_upper = memdump_addr + MEMDUMP_ROWS*MEMDUMP_COLS;
	}
	memdump_upper &= 0x00FFFFFF;

	while (memdump_addr < memdump_upper)
	{
		fprintf(debugOutput, "%6.6X: ", memdump_addr); /* print address */
		for (i = 0; i < MEMDUMP_COLS; i++)               /* print hex data */
			fprintf(debugOutput, "%2.2x ", STMemory_ReadByte(memdump_addr++));
		fprintf(debugOutput, "  ");                     /* print ASCII data */
		for (i = 0; i < MEMDUMP_COLS; i++)
		{
			c = STMemory_ReadByte(memdump_addr-MEMDUMP_COLS+i);
			if(!isprint((unsigned)c))
				c = NON_PRINT_CHAR;             /* non-printable as dots */
			fprintf(debugOutput,"%c", c);
		}
		fprintf(debugOutput, "\n");            /* newline */
	} /* while */
	fflush(debugOutput);

	return DEBUGGER_CMDCONT;
}


/**
 * Command: Write to memory, arg = starting address, followed by bytes.
 */
static int DebugCpu_MemWrite(int nArgc, char *psArgs[])
{
	int i, numBytes;
	Uint32 write_addr, d;
	unsigned char bytes[256]; /* store bytes */

	if (nArgc < 3)
	{
		DebugUI_PrintCmdHelp(psArgs[0]);
		return DEBUGGER_CMDDONE;
	}

	/* Read address */
	if (!Eval_Number(psArgs[1], &write_addr))
	{
		fprintf(stderr, "Bad address!\n");
		return DEBUGGER_CMDDONE;
	}

	write_addr &= 0x00FFFFFF;
	numBytes = 0;

	/* get bytes data */
	for (i = 2; i < nArgc; i++)
	{
		if (!Eval_Number(psArgs[i], &d) || d > 255)
		{
			fprintf(stderr, "Bad byte argument: '%s'!\n", psArgs[i]);
			return DEBUGGER_CMDDONE;
		}

		bytes[numBytes] = d & 0x0FF;
		numBytes++;
	}

	/* write the data */
	for (i = 0; i < numBytes; i++)
		STMemory_WriteByte(write_addr + i, bytes[i]);

	return DEBUGGER_CMDDONE;
}


/**
 * Command: Continue CPU emulation / single-stepping
 */
static int DebugCpu_Continue(int nArgc, char *psArgv[])
{
	int steps = 0;
	
	if (nArgc > 1)
	{
		steps = atoi(psArgv[1]);
	}
	if (steps <= 0)
	{
		nCpuSteps = 0;
		fprintf(stderr,"Returning to emulation...\n");
		return DEBUGGER_END;
	}
	nCpuSteps = steps;
	fprintf(stderr,"Returning to emulation for %i CPU instructions...\n", steps);
	return DEBUGGER_END;
}


/**
 * This function is called after each CPU instruction when debugging is enabled.
 */
void DebugCpu_Check(void)
{
	if (LOG_TRACE_LEVEL(TRACE_CPU_DISASM))
	{
		DebugCpu_ShowMatchedSymbol(M68000_GetPC());
	}
	if (nCpuActiveCBs)
	{
		if (BreakCond_MatchCpu())
			DebugUI();
	}
	if (nCpuSteps)
	{
		nCpuSteps -= 1;
		if (nCpuSteps == 0)
			DebugUI();
	}
}

/**
 * Should be called before returning back emulation to tell the CPU core
 * to call us after each instruction if "real-time" debugging like
 * breakpoints has been set.
 */
void DebugCpu_SetDebugging(void)
{
	if (nCpuActiveCBs || nCpuSteps)
		M68000_SetSpecial(SPCFLAG_DEBUGGER);
	else
		M68000_UnsetSpecial(SPCFLAG_DEBUGGER);
}


static const dbgcommand_t cpucommands[] =
{
	{ NULL, "CPU commands", NULL, NULL, NULL, false },
	{ DebugCpu_BreakAddr, "address", "a",
	  "set CPU PC address breakpoints",
	  BreakAddr_Description,
	  true	},
	{ DebugCpu_BreakCond, "breakpoint", "b",
	  "set/remove/list conditional CPU breakpoints",
	  BreakCond_Description,
	  true },
	{ DebugCpu_DisAsm, "disasm", "d",
	  "disassemble from PC, or given address",
	  "[<start address>[-<end address>]]\n"
	  "\tIf no address is given, this command disassembles from the last\n"
	  "\tposition or from current PC if no last position is available.",
	  false },
	{ DebugCpu_Register, "cpureg", "r",
	  "dump register values or set register to value",
	  "[REG=value]\n"
	  "\tSet CPU register to value or dumps all register if no parameter\n"
	  "\thas been specified.",
	  true },
	{ DebugCpu_MemDump, "memdump", "m",
	  "dump memory",
	  "[<start address>[-<end address>]]\n"
	  "\tdump memory at address or continue dump from previous address.",
	  false },
	{ DebugCpu_MemWrite, "memwrite", "w",
	  "write bytes to memory",
	  "address byte1 [byte2 ...]\n"
	  "\tWrite bytes to a memory address, bytes are space separated\n"
	  "\thexadecimals.",
	  false },
	{ DebugCpu_LoadBin, "loadbin", "l",
	  "load a file into memory",
	  "filename address\n"
	  "\tLoad the file <filename> into memory starting at <address>.",
	  false },
	{ DebugCpu_SaveBin, "savebin", "s",
	  "save memory to a file",
	  "filename address length\n"
	  "\tSave the memory block at <address> with given <length> to\n"
	  "\tthe file <filename>.",
	  false },
	{ Symbols_Command, "symbols", "",
	  "load CPU symbols & their addresses",
	  Symbols_Description,
	  false },
	{ DebugCpu_Continue, "cont", "c",
	  "continue emulation / CPU single-stepping",
	  "[steps]\n"
	  "\tLeave debugger and continue emulation for <steps> CPU instructions\n"
	  "\tor forever if no steps have been specified.",
	  false }
};


/**
 * Should be called when debugger is first entered to initialize
 * CPU debugging variables.
 * 
 * if you want disassembly or memdumping to start/continue from
 * specific address, you can set them here.  If disassembly
 * address is zero, disassembling starts from PC.
 * 
 * returns number of CPU commands and pointer to array of them.
 */
int DebugCpu_Init(const dbgcommand_t **table)
{
	memdump_addr = 0;
	disasm_addr = 0;
	
	*table = cpucommands;
	return ARRAYSIZE(cpucommands);
}

/**
 * Should be called when debugger is re-entered to reset
 * relevant CPU debugging variables.
 */
void DebugCpu_InitSession(void)
{
	disasm_addr = M68000_GetPC();
}
