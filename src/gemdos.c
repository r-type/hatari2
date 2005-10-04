/*
  Hatari - gemdos.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  GEMDOS intercept routines.
  These are used mainly for hard drive redirection of high level file routines.

  Now case is handled by using glob. See the function
  GemDOS_CreateHardDriveFileName for that. It also knows about symlinks.
  A filename is recognized on its eight first characters, do not try to
  push this too far, or you'll get weirdness ! (But I can even run programs
  directly from a mounted cd in lower cases, so I guess it's working well !).

  Bugs/things to fix:
  * RS232
  * rmdir routine, can't remove dir with files in it. (another tos/unix difference)
  * Fix bugs, there are probably a few lurking around in here..
*/
char Gemdos_rcsid[] = "Hatari $Id: gemdos.c,v 1.43 2005-10-04 15:31:52 thothy Exp $";

#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <glob.h>

#include "main.h"
#include "cart.h"
#include "tos.h"
#include "configuration.h"
#include "file.h"
#include "floppy.h"
#include "hdc.h"
#include "gemdos.h"
#include "m68000.h"
#include "memorySnapShot.h"
#include "misc.h"
#include "printer.h"
#include "rs232.h"
#include "stMemory.h"
#include "uae-cpu/hatari-glue.h"
#include "uae-cpu/maccess.h"


// #define GEMDOS_VERBOSE
// uncomment the following line to debug filename lookups on hd
// #define FILE_DEBUG 1

#define ENABLE_SAVING             /* Turn on saving stuff */

#define INVALID_HANDLE_VALUE -1

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

/* GLOB_ONLYDIR is a GNU extension for the glob() function and not defined
 * on some systems. We should probably use something different for this
 * case, but at the moment it we simply define it as 0... */
#ifndef GLOB_ONLYDIR
#warning GLOB_ONLYDIR was not defined.
#define GLOB_ONLYDIR 0
#endif


/* structure with all the drive-specific data for our emulated drives */
EMULATEDDRIVE **emudrives = NULL;

typedef struct
{
	BOOL bUsed;
	FILE *FileHandle;
	char szActualName[MAX_PATH];        /* used by F_DATIME (0x57) */
} FILE_HANDLE;

typedef struct
{
	BOOL bUsed;
	int  nentries;                      /* number of entries in fs directory */
	int  centry;                        /* current entry # */
	struct dirent **found;              /* legal files */
	char path[MAX_PATH];                /* sfirst path */
} INTERNAL_DTA;

FILE_HANDLE  FileHandles[MAX_FILE_HANDLES];
INTERNAL_DTA InternalDTAs[MAX_DTAS_FILES];
int DTAIndex;                               /* Circular index into above */
BOOL bInitGemDOS;                           /* Have we re-directed GemDOS vector to our own routines yet? */
DTA *pDTA;                                  /* Our GEMDOS hard drive Disk Transfer Address structure */
unsigned short int CurrentDrive;            /* Current drive (0=A,1=B,2=C etc...) */
Uint32 act_pd;                              /* Used to get a pointer to the current basepage */


#ifdef GEMDOS_VERBOSE
/* List of GEMDos functions... */
static const char *pszGemDOSNames[] =
{
	"Term",                 /*0x00*/
	"Conin",                /*0x01*/
	"ConOut",               /*0x02*/
	"Auxiliary Input",      /*0x03*/
	"Auxiliary Output",     /*0x04*/
	"Printer Output",       /*0x05*/
	"RawConIO",             /*0x06*/
	"Direct Conin no echo", /*0x07*/
	"Conin no echo",        /*0x08*/
	"Print line",           /*0x09*/
	"ReadLine",             /*0x0a*/
	"ConStat",              /*0x0b*/
	"",                     /*0x0c*/
	"",                     /*0x0d*/
	"SetDrv",               /*0x0e*/
	"",                     /*0x0f*/
	"Conout Stat",          /*0x10*/
	"PrtOut Stat",          /*0x11*/
	"Auxin Stat",           /*0x12*/
	"AuxOut Stat",          /*0x13*/
	"",                     /*0x14*/
	"",                     /*0x15*/
	"",                     /*0x16*/
	"",                     /*0x17*/
	"",                     /*0x18*/
	"Current Disk",         /*0x19*/
	"Set DTA",              /*0x1a*/
	"",                     /*0x1b*/
	"",                     /*0x1c*/
	"",                     /*0x1d*/
	"",                     /*0x1e*/
	"",                     /*0x1f*/
	"Super",                /*0x20*/
	"",                     /*0x21*/
	"",                     /*0x22*/
	"",                     /*0x23*/
	"",                     /*0x24*/
	"",                     /*0x25*/
	"",                     /*0x26*/
	"",                     /*0x27*/
	"",                     /*0x28*/
	"",                     /*0x29*/
	"Get Date",             /*0x2a*/
	"Set Date",             /*0x2b*/
	"Get Time",             /*0x2c*/
	"Set Time",             /*0x2d*/
	"",                     /*0x2e*/
	"Get DTA",              /*0x2f*/
	"Get Version Number",   /*0x30*/
	"Keep Process",         /*0x31*/
	"",                     /*0x32*/
	"",                     /*0x33*/
	"",                     /*0x34*/
	"",                     /*0x35*/
	"Get Disk Free Space",  /*0x36*/
	"",           /*0x37*/
	"",           /*0x38*/
	"MkDir",      /*0x39*/
	"RmDir",      /*0x3a*/
	"ChDir",      /*0x3b*/
	"Create",     /*0x3c*/
	"Open",       /*0x3d*/
	"Close",      /*0x3e*/
	"Read",       /*0x3f*/
	"Write",      /*0x40*/
	"UnLink",     /*0x41*/
	"LSeek",      /*0x42*/
	"Fattrib",    /*0x43*/
	"",           /*0x44*/
	"Dup",        /*0x45*/
	"Force",      /*0x46*/
	"GetDir",     /*0x47*/
	"Malloc",     /*0x48*/
	"MFree",      /*0x49*/
	"SetBlock",   /*0x4a*/
	"Exec",       /*0x4b*/
	"Term",       /*0x4c*/
	"",           /*0x4d*/
	"SFirst",     /*0x4e*/
	"SNext",      /*0x4f*/
	"",           /*0x50*/
	"",           /*0x51*/
	"",           /*0x52*/
	"",           /*0x53*/
	"",           /*0x54*/
	"",           /*0x55*/
	"Rename",     /*0x56*/
	"GSDTof"      /*0x57*/
};
#endif




/*-------------------------------------------------------*/
/*
  Routines to convert time and date to MSDOS format.
  Originally from the STonX emulator. (cheers!)
*/
static Uint16 GemDOS_Time2dos(time_t t)
{
	struct tm *x;
	x = localtime (&t);
	return (x->tm_sec>>1)|(x->tm_min<<5)|(x->tm_hour<<11);
}

static Uint16 GemDOS_Date2dos(time_t t)
{
	struct tm *x;
	x = localtime (&t);
	return x->tm_mday | ((x->tm_mon+1)<<5) | (((x->tm_year-80 > 0) ? x->tm_year-80 : 0) << 9);
}


/*-----------------------------------------------------------------------*/
/*
  Populate a DATETIME structure with file info
*/
static BOOL GemDOS_GetFileInformation(char *name, DATETIME *DateTime)
{
	struct stat filestat;
	int n;
	struct tm *x;

	n = stat(name, &filestat);
	if (n != 0)
		return FALSE;
	x = localtime( &filestat.st_mtime );

	DateTime->word1 = 0;
	DateTime->word2 = 0;

	DateTime->word1 |= (x->tm_mday & 0x1F);         /* 5 bits */
	DateTime->word1 |= (x->tm_mon & 0x0F)<<5;       /* 4 bits */
	DateTime->word1 |= (((x->tm_year-80>0)?x->tm_year-80:0) & 0x7F)<<9;      /* 7 bits*/

	DateTime->word2 |= (x->tm_sec & 0x1F);          /* 5 bits */
	DateTime->word2 |= (x->tm_min & 0x3F)<<5;       /* 6 bits */
	DateTime->word2 |= (x->tm_hour & 0x1F)<<11;     /* 5 bits */

	return TRUE;
}


/*-----------------------------------------------------------------------*/
/*
  Covert from FindFirstFile/FindNextFile attribute to GemDOS format
*/
static unsigned char GemDOS_ConvertAttribute(mode_t mode)
{
	unsigned char Attrib = 0;

	/* Directory attribute */
	if (S_ISDIR(mode))
		Attrib |= GEMDOS_FILE_ATTRIB_SUBDIRECTORY;

	/* Read-only attribute */
	if (!(mode & S_IWUSR))
		Attrib |= GEMDOS_FILE_ATTRIB_READONLY;
	
	/* TODO: Other attributes like GEMDOS_FILE_ATTRIB_HIDDEN ? */

	return Attrib;
}


/*-----------------------------------------------------------------------*/
/*
  Populate the DTA buffer with file info
*/
static BOOL PopulateDTA(char *path, struct dirent *file)
{
	char tempstr[MAX_PATH];
	struct stat filestat;
	int n;

	sprintf(tempstr, "%s/%s", path, file->d_name);
	n = stat(tempstr, &filestat);
	if (n != 0)
		return FALSE;   /* return on error */

	if (!pDTA)
		return FALSE;   /* no DTA pointer set */
	Misc_strupr(file->d_name);    /* convert to atari-style uppercase */
	strncpy(pDTA->dta_name,file->d_name,TOS_NAMELEN); /* FIXME: better handling of long file names */
	do_put_mem_long(pDTA->dta_size, filestat.st_size);
	do_put_mem_word(pDTA->dta_time, GemDOS_Time2dos(filestat.st_mtime));
	do_put_mem_word(pDTA->dta_date, GemDOS_Date2dos(filestat.st_mtime));
	pDTA->dta_attrib = GemDOS_ConvertAttribute(filestat.st_mode);

	return TRUE;
}


/*-----------------------------------------------------------------------*/
/*
  Clear a used DTA structure.
*/
static void ClearInternalDTA(void)
{
	int i;

	/* clear the old DTA structure */
	if (InternalDTAs[DTAIndex].found != NULL)
	{
		for (i=0; i < InternalDTAs[DTAIndex].nentries; i++)
			free(InternalDTAs[DTAIndex].found[i]);
		free(InternalDTAs[DTAIndex].found);
		InternalDTAs[DTAIndex].found = NULL;
	}
	InternalDTAs[DTAIndex].nentries = 0;
	InternalDTAs[DTAIndex].bUsed = FALSE;
}


/*-----------------------------------------------------------------------*/
/*
  Match a file to a dir mask.
*/
static int match (char *pat, char *name)
{
	char *p=pat, *n=name;

	if (name[0] == '.')
		return FALSE;                   /* no .* files */
	if (strcmp(pat,"*.*")==0)
		return TRUE;
	if (strcasecmp(pat,name)==0)
		return TRUE;

	for (;*n;)
	{
		if (*p=='*')
		{
			while (*n && *n != '.')
				n++;
			p++;
		}
		else
		{
			if (*p=='?' && *n)
			{
				n++;
				p++;
			}
			else
			{
				if (toupper(*p++) != toupper(*n++))
					return FALSE;
			}
		}
	}

	return (*p == 0);     /* The name matches the pattern if it ends here, too */
}


/*-----------------------------------------------------------------------*/
/*
  Parse directory from sfirst mask
  - e.g.: input:  "hdemudir/auto/mask*.*" outputs: "hdemudir/auto"
*/
static void fsfirst_dirname(char *string, char *new)
{
	int i=0;

	sprintf(new, string);
	/* convert to front slashes. */
	i=0;
	while (new[i] != '\0')
	{
		if (new[i] == '\\')
			new[i] = '/';
		i++;
	}
	while (string[i] != '\0')
	{
		new[i] = string[i];
		i++;
	} /* find end of string */
	while (new[i] != '/')
		i--; /* find last slash */
	new[i] = '\0';

}

/*-----------------------------------------------------------------------*/
/*
  Parse directory mask, e.g. "*.*"
*/
static void fsfirst_dirmask(char *string, char *new)
{
	int i=0, j=0;

	while (string[i] != '\0')
		i++;   /* go to end of string */
	while (string[i] != '/')
		i--;   /* find last slash */
	i++;
	while (string[i] != '\0')
		new[j++] = string[i++]; /* go to end of string */
	new[j++] = '\0';
}


/*-----------------------------------------------------------------------*/
/*
  Initialize GemDOS/PC file system
*/
void GemDOS_Init(void)
{
	int i;
	bInitGemDOS = FALSE;

	/* Clear handles structure */
	memset(FileHandles, 0, sizeof(FILE_HANDLE)*MAX_FILE_HANDLES);
	/* Clear DTAs */
	for(i=0; i<MAX_DTAS_FILES; i++)
	{
		InternalDTAs[i].bUsed = FALSE;
		InternalDTAs[i].nentries = 0;
		InternalDTAs[i].found = NULL;
	}
	DTAIndex = 0;
}


/*-----------------------------------------------------------------------*/
/*
  Reset GemDOS file system
*/
void GemDOS_Reset(void)
{
	int i;

	/* Init file handles table */
	for (i=0; i<MAX_FILE_HANDLES; i++)
	{
		/* Was file open? If so close it */
		if (FileHandles[i].bUsed)
			fclose(FileHandles[i].FileHandle);

		FileHandles[i].FileHandle = NULL;
		FileHandles[i].bUsed = FALSE;
	}

	for (DTAIndex = 0; DTAIndex < MAX_DTAS_FILES; DTAIndex++)
	{
		ClearInternalDTA();
	}
	DTAIndex = 0;

	/* Reset */
	bInitGemDOS = FALSE;
	CurrentDrive = nBootDrive;
	pDTA = NULL;
}


/*-----------------------------------------------------------------------*/
/*
  Initialize a GEMDOS drive.
  Only 1 emulated drive allowed, as of yet.
*/
void GemDOS_InitDrives(void)
{
	int i;

	/* intialize data for harddrive emulation: */
	if (!GEMDOS_EMU_ON)
	{
		emudrives = malloc( MAX_HARDDRIVES*sizeof(EMULATEDDRIVE *) );
		for(i=0; i<MAX_HARDDRIVES; i++)
			emudrives[0] = malloc( sizeof(EMULATEDDRIVE) );
	}

	for(i=0; i<MAX_HARDDRIVES; i++)
	{
		/* set emulation directory string */
		strcpy(emudrives[i]->hd_emulation_dir, ConfigureParams.HardDisk.szHardDiskDirectories[i]);

		/* remove trailing slash, if any in the directory name */
		File_CleanFileName(emudrives[i]->hd_emulation_dir);

		/* initialize current directory string, too (initially the same as hd_emulation_dir) */
		strcpy(emudrives[i]->fs_currpath, emudrives[i]->hd_emulation_dir);
		strcat(emudrives[i]->fs_currpath, "/");    /* Needs trailing slash! */

		/* set drive to 2 + number of ACSI partitions */
		emudrives[i]->hd_letter = 2 + nPartitions + i;

		nNumDrives += 1;

		fprintf(stderr, "Hard drive emulation, %c: <-> %s\n",
				emudrives[i]->hd_letter + 'A', emudrives[i]->hd_emulation_dir);
	}
}


/*-----------------------------------------------------------------------*/
/*
  Un-init the GEMDOS drive
*/
void GemDOS_UnInitDrives(void)
{
	int i;

	GemDOS_Reset();        /* Close all open files on emulated drive*/

	if (GEMDOS_EMU_ON)
	{
		for(i=0; i<MAX_HARDDRIVES; i++)
		{
			free(emudrives[i]);    /* Release memory */
			nNumDrives -= 1;
		}

		free(emudrives);
		emudrives = NULL;
	}
}


/*-----------------------------------------------------------------------*/
/*
  Save/Restore snapshot of local variables('MemorySnapShot_Store' handles type)
*/
void GemDOS_MemorySnapShot_Capture(BOOL bSave)
{
	unsigned int Addr;
	int i;

	/* Save/Restore details */
	MemorySnapShot_Store(&DTAIndex,sizeof(DTAIndex));
	MemorySnapShot_Store(&bInitGemDOS,sizeof(bInitGemDOS));
	if (bSave)
	{
		Addr = (unsigned int)((Uint8 *)pDTA - STRam);
		MemorySnapShot_Store(&Addr,sizeof(Addr));
	}
	else
	{
		MemorySnapShot_Store(&Addr,sizeof(Addr));
		pDTA = (DTA *)(STRam + Addr);
	}
	MemorySnapShot_Store(&CurrentDrive,sizeof(CurrentDrive));
	/* Don't save file handles as files may have changed which makes
	   it impossible to get a valid handle back */
	if (!bSave)
	{
		/* Clear file handles  */
		for(i=0; i<MAX_FILE_HANDLES; i++)
		{
			FileHandles[i].FileHandle = NULL;
			FileHandles[i].bUsed = FALSE;
		}
	}
}


/*-----------------------------------------------------------------------*/
/*
  Return free PC file handle table index, or -1 if error
*/
static int GemDOS_FindFreeFileHandle(void)
{
	int i;

	/* Scan our file list for free slot */
	for(i=0; i<MAX_FILE_HANDLES; i++)
	{
		if (!FileHandles[i].bUsed)
			return i;
	}

	/* Cannot open any more files, return error */
	return -1;
}

/*-----------------------------------------------------------------------*/
/*
  Check ST handle is within our table range, return TRUE if not
*/
static BOOL GemDOS_IsInvalidFileHandle(int Handle)
{
	BOOL bInvalidHandle=FALSE;

	/* Check handle was valid with our handle table */
	if ((Handle < 0) || (Handle >= MAX_FILE_HANDLES))
		bInvalidHandle = TRUE;
	else if (!FileHandles[Handle].bUsed)
		bInvalidHandle = TRUE;

	return bInvalidHandle;
}

/*-----------------------------------------------------------------------*/
/*
  Find drive letter from a filename, eg C,D... and return as drive ID(C:2, D:3...)
  returns the current drive number if none is specified.
*/
static int GemDOS_FindDriveNumber(char *pszFileName)
{
	/* Does have 'A:' or 'C:' etc.. at start of string? */
	if ((pszFileName[0] != '\0') && (pszFileName[1] == ':'))
	{
		if ((pszFileName[0] >= 'a') && (pszFileName[0] <= 'z'))
			return (pszFileName[0]-'a');
		else if ((pszFileName[0] >= 'A') && (pszFileName[0] <= 'Z'))
			return (pszFileName[0]-'A');
	}

	return CurrentDrive;
}

/*-----------------------------------------------------------------------*/
/*
  Return drive ID(C:2, D:3 etc...) or -1 if not one of our emulation hard-drives
*/
static int GemDOS_IsFileNameAHardDrive(char *pszFileName)
{
	int DriveLetter;

	/* Do we even have a hard-drive? */
	if (GEMDOS_EMU_ON)
	{
		/* Find drive letter (as number) */
		DriveLetter = GemDOS_FindDriveNumber(pszFileName);
		/* add support for multiple drives here.. */
		if (DriveLetter == emudrives[0]->hd_letter)
			return DriveLetter;
	}
	/* Not a high-level redirected drive, let TOS handle it */
	return -1;
}


/*-----------------------------------------------------------------------*/
/*
  Returns the length of the basename of the file passed in parameter
   (ie the file without extension)
*/
static int baselen(char *s)
{
	char *ext = strchr(s,'.');
	if (ext)
		return ext-s;
	return strlen(s);
}

/*-----------------------------------------------------------------------*/
/*
  Use hard-drive directory, current ST directory and filename to create full path
*/
void GemDOS_CreateHardDriveFileName(int Drive, const char *pszFileName, char *pszDestName)
{
	char *s,*start;

	if (pszFileName[0] == '\0')
		return; /* check for valid string */

	/* case full filename "C:\foo\bar" */
	s=pszDestName;
	start=NULL;

	if (pszFileName[1] == ':')
	{
		sprintf(pszDestName, "%s%s", emudrives[0]->hd_emulation_dir, File_RemoveFileNameDrive(pszFileName));
	}
	/* case referenced from root:  "\foo\bar" */
	else if (pszFileName[0] == '\\')
	{
		sprintf(pszDestName, "%s%s", emudrives[0]->hd_emulation_dir, pszFileName);
	}
	/* case referenced from current directory */
	else
	{
		sprintf(pszDestName, "%s%s",  emudrives[0]->fs_currpath, pszFileName);
		start = pszDestName + strlen(emudrives[0]->fs_currpath)-1;
	}

	/* convert to front slashes. */
	while((s = strchr(s+1,'\\')))
	{
		if (!start)
		{
			start = s;
			continue;
		}
		{
			glob_t globbuf;
			char old1,old2,dest[256];
			int len, found, base_len;
			unsigned int j;

			*start++ = '/';
			old1 = *start;
			*start++ = '*';
			old2 = *start;
			*start = 0;
			glob(pszDestName,GLOB_ONLYDIR,NULL,&globbuf);
			*start-- = old2;
			*start = old1;
			*s = 0;
			len = strlen(pszDestName);
			base_len = baselen(start);
			found = 0;
			for (j=0; j<globbuf.gl_pathc; j++)
			{
				/* If we search for a file of at least 8 characters, then it might
				   be a longer filename since the ST can access only the first 8
				   characters. If not, then it's a precise match (with case). */
				if (!(base_len < 8 ? strcasecmp(globbuf.gl_pathv[j],pszDestName) :
						strncasecmp(globbuf.gl_pathv[j],pszDestName,len)))
				{
					/* we found a matching name... */
					sprintf(dest,"%s%c%s",globbuf.gl_pathv[j],'/',s+1);
					strcpy(pszDestName,dest);
					j = globbuf.gl_pathc;
					found = 1;
				}
			}
			globfree(&globbuf);
			if (!found)
			{
				/* didn't find it. Let's try normal files (it might be a symlink) */
				*start++ = '*';
				*start = 0;
				glob(pszDestName,0,NULL,&globbuf);
				*start-- = old2;
				*start = old1;
				for (j=0; j<globbuf.gl_pathc; j++)
				{
					if (!strncasecmp(globbuf.gl_pathv[j],pszDestName,len))
					{
						/* we found a matching name... */
						sprintf(dest,"%s%c%s",globbuf.gl_pathv[j],'/',s+1);
						strcpy(pszDestName,dest);
						j = globbuf.gl_pathc;
						found = 1;
					}
				}
				globfree(&globbuf);
				if (!found)
				{           /* really nothing ! */
					*s = '/';
					fprintf(stderr,"no path for %s\n",pszDestName);
				}
			}
		}
		start = s;
	}

	if (!start)
		start = strrchr(pszDestName,'/'); // path already converted ?

	if (start)
	{
		*start++ = '/';     /* in case there was only 1 anti slash */
		if (*start && !strchr(start,'?') && !strchr(start,'*'))
		{
			/* We have a complete name after the path, not a wildcard */
			glob_t globbuf;
			char old1,old2;
			int len, found, base_len;
			unsigned int j;

			old1 = *start;
			*start++ = '*';
			old2 = *start;
			*start = 0;
			glob(pszDestName,0,NULL,&globbuf);
			*start-- = old2;
			*start = old1;
			len = strlen(pszDestName);
			base_len = baselen(start);
			found = 0;
			for (j=0; j<globbuf.gl_pathc; j++)
			{
				/* If we search for a file of at least 8 characters, then it might
				   be a longer filename since the ST can access only the first 8
				   characters. If not, then it's a precise match (with case). */
				if (!(base_len < 8 ? strcasecmp(globbuf.gl_pathv[j],pszDestName) :
						strncasecmp(globbuf.gl_pathv[j],pszDestName,len)))
				{
					/* we found a matching name... */
					strcpy(pszDestName,globbuf.gl_pathv[j]);
					j = globbuf.gl_pathc;
					found = 1;
				}
			}
#if FILE_DEBUG
			if (!found)
			{
				/* It's often normal, the gem uses this to test for existence */
				/* of desktop.inf or newdesk.inf for example. */
				fprintf(stderr,"didn't find filename %s\n",pszDestName);
			}
#endif
			globfree(&globbuf);
		}
	}

#if FILE_DEBUG
	fprintf(stderr,"conv %s -> %s\n",pszFileName,pszDestName);
#endif
}


/*-----------------------------------------------------------------------*/
/*
  GEMDOS Cauxin
  Call 0x3
*/
#if 0
static BOOL GemDOS_Cauxin(Uint32 Params)
{
	unsigned char Char;

	/* Wait here until a character is ready */
	while(!RS232_GetStatus())
		;

	/* And read character */
	RS232_ReadBytes(&Char,1);
	Regs[REG_D0] = Char;

	return TRUE;
}
#endif

/*-----------------------------------------------------------------------*/
/*
  GEMDOS Cauxout
  Call 0x4
*/
#if 0
static BOOL GemDOS_Cauxout(Uint32 Params)
{
	unsigned char Char;

	/* Send character to RS232 */
	Char = STMemory_ReadWord(Params+SIZE_WORD);
	RS232_TransferBytesTo(&Char,1);

	return TRUE;
}
#endif

/*-----------------------------------------------------------------------*/
/*
  GEMDOS Cprnout
  Call 0x5
*/
static BOOL GemDOS_Cprnout(Uint32 Params)
{
	unsigned char Char;

	/* Send character to printer(or file) */
	Char = STMemory_ReadWord(Params+SIZE_WORD);
	Printer_TransferByteTo(Char);
	Regs[REG_D0] = -1;                /* Printer OK */

	return TRUE;
}

/*-----------------------------------------------------------------------*/
/*
  GEMDOS Set drive (0=A,1=B,2=C etc...)
  Call 0xE
*/
static BOOL GemDOS_SetDrv(Uint32 Params)
{
	/* Read details from stack for our own use */
	CurrentDrive = STMemory_ReadWord(Params+SIZE_WORD);

	/* Still re-direct to TOS */
	return FALSE;
}

/*-----------------------------------------------------------------------*/
/*
  GEMDOS Cprnos
  Call 0x11
*/
static BOOL GemDOS_Cprnos(Uint32 Params)
{
	/* pritner status depends if printing is enabled or not... */
	if (ConfigureParams.Printer.bEnablePrinting)
		Regs[REG_D0] = -1;              /* Printer OK */
	else
		Regs[REG_D0] = 0;               /* printer not ready if printing disabled */

	return TRUE;
}

/*-----------------------------------------------------------------------*/
/*
  GEMDOS Cauxis
  Call 0x12
*/
#if 0
static BOOL GemDOS_Cauxis(Uint32 Params)
{
	/* Read our RS232 state */
	if (RS232_GetStatus())
		Regs[REG_D0] = -1;              /* Chars waiting */
	else
		Regs[REG_D0] = 0;

	return TRUE;
}
#endif

/*-----------------------------------------------------------------------*/
/*
  GEMDOS Cauxos
  Call 0x13
*/
#if 0
static BOOL GemDOS_Cauxos(Uint32 Params)
{
	Regs[REG_D0] = -1;                /* Device ready */

	return TRUE;
}
#endif

/*-----------------------------------------------------------------------*/
/*
  GEMDOS Set Disk Transfer Address (DTA)
  Call 0x1A
*/
static BOOL GemDOS_SetDTA(Uint32 Params)
{
	/* Look up on stack to find where DTA is! Store as PC pointer */
	pDTA = (DTA *)STRAM_ADDR(STMemory_ReadLong(Params+SIZE_WORD));

	return FALSE;
}

/*-----------------------------------------------------------------------*/
/*
  GEMDOS Dfree Free disk space.
  Call 0x39
*/
static BOOL GemDOS_DFree(Uint32 Params)
{
	int Drive;
	Uint32 Address;

	Address = STMemory_ReadLong(Params+SIZE_WORD);
	Drive = STMemory_ReadWord(Params+SIZE_WORD+SIZE_LONG);
	/* is it our drive? */
	if ((Drive == 0 && CurrentDrive >= 2) || Drive >= 3)
	{
		/* FIXME: Report actual free drive space */

		STMemory_WriteLong(Address,  10*2048);           /* free clusters (mock 10 Mb) */
		STMemory_WriteLong(Address+SIZE_LONG, 50*2048 ); /* total clusters (mock 50 Mb) */

		STMemory_WriteLong(Address+SIZE_LONG*2, 512 );   /* bytes per sector */
		STMemory_WriteLong(Address+SIZE_LONG*3, 1 );     /* sectors per cluster */
		return TRUE;
	}
	else
		return FALSE; /* redirect to TOS */
}

/*-----------------------------------------------------------------------*/
/*
  GEMDOS MkDir
  Call 0x39
*/
static BOOL GemDOS_MkDir(Uint32 Params)
{
	char szDirPath[MAX_PATH];
	char *pDirName;
	int Drive;

	/* Find directory to make */
	pDirName = (char *)STRAM_ADDR(STMemory_ReadLong(Params+SIZE_WORD));

	Drive = GemDOS_IsFileNameAHardDrive(pDirName);

	if (ISHARDDRIVE(Drive))
	{
		/* Copy old directory, as if calls fails keep this one */
		GemDOS_CreateHardDriveFileName(Drive,pDirName,szDirPath);

		/* Attempt to make directory */
		if ( mkdir(szDirPath, 0755)==0 )
			Regs[REG_D0] = GEMDOS_EOK;
		else
			Regs[REG_D0] = GEMDOS_EACCDN;        /* Access denied */

		return TRUE;
	}
	return FALSE;
}

/*-----------------------------------------------------------------------*/
/*
  GEMDOS RmDir
  Call 0x3A
*/
static BOOL GemDOS_RmDir(Uint32 Params)
{
	char szDirPath[MAX_PATH];
	char *pDirName;
	int Drive;

	/* Find directory to make */
	pDirName = (char *)STRAM_ADDR(STMemory_ReadLong(Params+SIZE_WORD));
	Drive = GemDOS_IsFileNameAHardDrive(pDirName);
	if (ISHARDDRIVE(Drive))
	{
		/* Copy old directory, as if calls fails keep this one */
		GemDOS_CreateHardDriveFileName(Drive,pDirName,szDirPath);

		/* Attempt to make directory */
		if ( rmdir(szDirPath)==0 )
			Regs[REG_D0] = GEMDOS_EOK;
		else
			Regs[REG_D0] = GEMDOS_EACCDN;        /* Access denied */

		return TRUE;
	}
	return FALSE;
}

/*-----------------------------------------------------------------------*/
/*
  GEMDOS ChDir
  Call 0x3B
*/
static BOOL GemDOS_ChDir(Uint32 Params)
{
	char szDirPath[MAX_PATH];
	char *pDirName;
	int Drive;

	/* Find new directory */
	pDirName = (char *)STRAM_ADDR(STMemory_ReadLong(Params+SIZE_WORD));
#if FILE_DEBUG

	fprintf(stderr,"chdir %s\n",pDirName);
#endif

	Drive = GemDOS_IsFileNameAHardDrive(pDirName);

	if (ISHARDDRIVE(Drive))
	{

		struct stat buf;

		GemDOS_CreateHardDriveFileName(Drive,pDirName,szDirPath);
		if (stat(szDirPath,&buf))
		{ /* error */
			Regs[REG_D0] = GEMDOS_EPTHNF;
			return TRUE;
		}

		strcat(szDirPath, "/");

		/* remove any trailing slashes */
		if (szDirPath[strlen(szDirPath)-2]=='/')
			szDirPath[strlen(szDirPath)-1] = '\0';     /* then remove it! */

		strcpy(emudrives[0]->fs_currpath, szDirPath);
		Regs[REG_D0] = GEMDOS_EOK;
		return TRUE;
	}

	return FALSE;
}

/*-----------------------------------------------------------------------*/
/*
  GEMDOS Create file
  Call 0x3C
*/
static BOOL GemDOS_Create(Uint32 Params)
{
	char szActualFileName[MAX_PATH];
	char *pszFileName;
	const char *rwflags[] =
		{
			"w+", /* read / write (truncate if exists) */
			"wb"  /* write only */
		};
	int Drive,Index,Mode;

	/* Find filename */
	pszFileName = (char *)STRAM_ADDR(STMemory_ReadLong(Params+SIZE_WORD));
	Mode = STMemory_ReadWord(Params+SIZE_WORD+SIZE_LONG);
	Drive = GemDOS_IsFileNameAHardDrive(pszFileName);
	if (ISHARDDRIVE(Drive))
	{
		/* And convert to hard drive filename */
		GemDOS_CreateHardDriveFileName(Drive,pszFileName,szActualFileName);

		/* Find slot to store file handle, as need to return WORD handle for ST */
		Index = GemDOS_FindFreeFileHandle();
		if (Index==-1)
		{
			/* No free handles, return error code */
			Regs[REG_D0] = GEMDOS_ENHNDL;       /* No more handles */
			return TRUE;
		}
		else
		{
#ifdef ENABLE_SAVING

			FileHandles[Index].FileHandle = fopen(szActualFileName, rwflags[Mode&0x01]);

			if (FileHandles[Index].FileHandle != NULL)
			{
				/* Tag handle table entry as used and return handle */
				FileHandles[Index].bUsed = TRUE;
				Regs[REG_D0] = Index+BASE_FILEHANDLE;  /* Return valid ST file handle from range 6 to 45! (ours start from 0) */
				return TRUE;
			}
			else
			{
				Regs[REG_D0] = GEMDOS_EFILNF;     /* File not found */
				return TRUE;
			}
#else
			Regs[REG_D0] = GEMDOS_EFILNF;         /* File not found */
			return TRUE;
#endif

		}
	}

	return FALSE;
}

/*-----------------------------------------------------------------------*/
/*
  GEMDOS Open file
  Call 0x3D
*/
static BOOL GemDOS_Open(Uint32 Params)
{
	char szActualFileName[MAX_PATH];
	char *pszFileName;
	const char *open_modes[] =
		{ "rb", "wb", "r+", "rb"
		}
		;  /* convert atari modes to stdio modes */
	int Drive,Index,Mode;

	/* Find filename */
	pszFileName = (char *)STRAM_ADDR(STMemory_ReadLong(Params+SIZE_WORD));
	Mode = STMemory_ReadWord(Params+SIZE_WORD+SIZE_LONG);
	Drive = GemDOS_IsFileNameAHardDrive(pszFileName);

	if (ISHARDDRIVE(Drive))
	{
		/* And convert to hard drive filename */
		GemDOS_CreateHardDriveFileName(Drive,pszFileName,szActualFileName);
		/* Find slot to store file handle, as need to return WORD handle for ST  */
		Index = GemDOS_FindFreeFileHandle();
		if (Index == -1)
		{
			/* No free handles, return error code */
			Regs[REG_D0] = GEMDOS_ENHNDL;       /* No more handles */
			return TRUE;
		}

		/* Open file */
		FileHandles[Index].FileHandle =  fopen(szActualFileName, open_modes[Mode&0x03]);

		sprintf(FileHandles[Index].szActualName,"%s",szActualFileName);

		if (FileHandles[Index].FileHandle != NULL)
		{
			/* Tag handle table entry as used and return handle */
			FileHandles[Index].bUsed = TRUE;
			Regs[REG_D0] = Index+BASE_FILEHANDLE;  /* Return valid ST file handle from range 6 to 45! (ours start from 0) */
			return TRUE;
		}
		Regs[REG_D0] = GEMDOS_EFILNF;     /* File not found/ error opening */
		return TRUE;
	}

	return FALSE;
}

/*-----------------------------------------------------------------------*/
/*
  GEMDOS Close file
  Call 0x3E
*/
static BOOL GemDOS_Close(Uint32 Params)
{
	int Handle;

	/* Find our handle - may belong to TOS */
	Handle = STMemory_ReadWord(Params+SIZE_WORD)-BASE_FILEHANDLE;

	/* Check handle was valid */
	if (GemDOS_IsInvalidFileHandle(Handle))
	{
		/* No assume was TOS */
		return FALSE;
	}
	else
	{
		/* Close file and free up handle table */
		fclose(FileHandles[Handle].FileHandle);
		FileHandles[Handle].bUsed = FALSE;
		/* Return no error */
		Regs[REG_D0] = GEMDOS_EOK;
		return TRUE;
	}
}

/*-----------------------------------------------------------------------*/
/*
  GEMDOS Read file
  Call 0x3F
*/
static BOOL GemDOS_Read(Uint32 Params)
{
	char *pBuffer;
	unsigned long nBytesRead,Size,CurrentPos,FileSize;
	long nBytesLeft;
	int Handle;

	/* Read details from stack */
	Handle = STMemory_ReadWord(Params+SIZE_WORD)-BASE_FILEHANDLE;
	Size = STMemory_ReadLong(Params+SIZE_WORD+SIZE_WORD);
	pBuffer = (char *)STRAM_ADDR(STMemory_ReadLong(Params+SIZE_WORD+SIZE_WORD+SIZE_LONG));

	/* Check handle was valid */
	if (GemDOS_IsInvalidFileHandle(Handle))
	{
		/* No -  assume was TOS */
		return FALSE;
	}
	else
	{

		/* To quick check to see where our file pointer is and how large the file is */
		CurrentPos = ftell(FileHandles[Handle].FileHandle);
		fseek(FileHandles[Handle].FileHandle, 0, SEEK_END);
		FileSize = ftell(FileHandles[Handle].FileHandle);
		fseek(FileHandles[Handle].FileHandle, CurrentPos, SEEK_SET);

		nBytesLeft = FileSize-CurrentPos;

		/* Check for End Of File */
		if (nBytesLeft == 0)
		{
			/* FIXME: should we return zero (bytes read) or an error? */
			Regs[REG_D0] = 0;
			return TRUE;
		}
		else
		{
			/* Limit to size of file to prevent errors */
			if (Size > FileSize)
				Size = FileSize;
			/* And read data in */
			nBytesRead = fread(pBuffer, 1, Size, FileHandles[Handle].FileHandle);

			/* Return number of bytes read */
			Regs[REG_D0] = nBytesRead;

			return TRUE;
		}
	}
}

/*-----------------------------------------------------------------------*/
/*
  GEMDOS Write file
  Call 0x40
*/
static BOOL GemDOS_Write(Uint32 Params)
{
	char *pBuffer;
	unsigned long Size,nBytesWritten;
	int Handle;

#ifdef ENABLE_SAVING
	/* Read details from stack */
	Handle = STMemory_ReadWord(Params+SIZE_WORD)-BASE_FILEHANDLE;
	Size = STMemory_ReadLong(Params+SIZE_WORD+SIZE_WORD);
	pBuffer = (char *)STRAM_ADDR(STMemory_ReadLong(Params+SIZE_WORD+SIZE_WORD+SIZE_LONG));

	/* Check if handle was not invalid */
	if (!GemDOS_IsInvalidFileHandle(Handle))
	{
		nBytesWritten = fwrite(pBuffer, 1, Size, FileHandles[Handle].FileHandle);
		if (nBytesWritten >= 0)
		{

			Regs[REG_D0] = nBytesWritten;      /* OK */
		}
		else
			Regs[REG_D0] = GEMDOS_EACCDN;      /* Access denied(ie read-only) */

		return TRUE;
	}
#endif

	return FALSE;
}

/*-----------------------------------------------------------------------*/
/*
  GEMDOS UnLink (Delete) file
  Call 0x41
*/
static BOOL GemDOS_UnLink(Uint32 Params)
{
#ifdef ENABLE_SAVING
	char szActualFileName[MAX_PATH];
	char *pszFileName;
	int Drive;

	/* Find filename */
	pszFileName = (char *)STRAM_ADDR(STMemory_ReadLong(Params+SIZE_WORD));
	Drive = GemDOS_IsFileNameAHardDrive(pszFileName);
	if (ISHARDDRIVE(Drive))
	{
		/* And convert to hard drive filename */
		GemDOS_CreateHardDriveFileName(Drive,pszFileName,szActualFileName);

		/* Now delete file?? */
		if ( unlink(szActualFileName)==0 )
			Regs[REG_D0] = GEMDOS_EOK;          /* OK */
		else
			Regs[REG_D0] = GEMDOS_EFILNF;       /* File not found */

		return TRUE;
	}
#endif

	return FALSE;
}

/*-----------------------------------------------------------------------*/
/*
  GEMDOS File seek
  Call 0x42
*/
static BOOL GemDOS_LSeek(Uint32 Params)
{
	long Offset;
	int Handle,Mode;

	/* Read details from stack */
	Offset = (long)STMemory_ReadLong(Params+SIZE_WORD);
	Handle = STMemory_ReadWord(Params+SIZE_WORD+SIZE_LONG)-BASE_FILEHANDLE;
	Mode = STMemory_ReadWord(Params+SIZE_WORD+SIZE_LONG+SIZE_WORD);

	/* Check handle was valid */
	if (GemDOS_IsInvalidFileHandle(Handle))
	{
		/* No assume was TOS */
		return FALSE;
	}
	else
	{
		/* Return offset from start of file */
		fseek(FileHandles[Handle].FileHandle, Offset, Mode);
		Regs[REG_D0] = ftell(FileHandles[Handle].FileHandle);
		return TRUE;
	}
}


/*-----------------------------------------------------------------------*/
/*
  GEMDOS Fattrib() - get or set file attributes
  Call 0x43
*/
static BOOL GemDOS_Fattrib(Uint32 Params)
{
	char sActualFileName[MAX_PATH];
	char *psFileName;
	int nDrive;
	int nRwFlag, nAttrib;

	/* Find filename */
	psFileName = (char *)STRAM_ADDR(STMemory_ReadLong(Params+SIZE_WORD));
	nDrive = GemDOS_IsFileNameAHardDrive(psFileName);

	nRwFlag = STMemory_ReadWord(Params+SIZE_WORD+SIZE_LONG);
	nAttrib = STMemory_ReadWord(Params+SIZE_WORD+SIZE_LONG+SIZE_WORD);

#ifdef GEMDOS_VERBOSE
	fprintf(stderr,"Fattrib('%s', %d, 0x%x)\n",psFileName, nRwFlag, nAttrib);
#endif

	if (ISHARDDRIVE(nDrive))
	{
		struct stat FileStat;

		/* Convert to hard drive filename */
		GemDOS_CreateHardDriveFileName(nDrive, psFileName, sActualFileName);

		if (stat(sActualFileName, &FileStat) != 0)
		{
			Regs[REG_D0] = GEMDOS_EFILNF;         /* File not found */
			return TRUE;
		}

		if (nRwFlag == 0)
		{
			/* Read attributes */
			Regs[REG_D0] = GemDOS_ConvertAttribute(FileStat.st_mode);
			return TRUE;
		}
		else
		{
			/* Write attributes */
			Regs[REG_D0] = GEMDOS_EACCDN;         /* Acces denied */
			return TRUE;
		}
	}

	return FALSE;
}


/*-----------------------------------------------------------------------*/
/*
  GEMDOS Get Directory
  Call 0x47
*/
static int GemDOS_GetDir(Uint32 Params)
{
	Uint32 Address;
	Uint16 Drive;

	Address = STMemory_ReadLong(Params+SIZE_WORD);
	Drive = STMemory_ReadWord(Params+SIZE_WORD+SIZE_LONG);
	/* is it our drive? */
	if ((Drive == 0 && CurrentDrive >= 2) || Drive >= 3)
	{
		char path[MAX_PATH];
		int i,len,c;

		Regs[REG_D0] = GEMDOS_EOK;          /* OK */
		strcpy(path,&emudrives[0]->fs_currpath[strlen(emudrives[0]->hd_emulation_dir)]);
		// convertit en path st (dos)
		len = strlen(path)-1;
		path[len] = 0;
		for (i = 0; i <= len; i++)
		{
			c = path[i];
			STMemory_WriteByte(Address+i, (c=='/' ? '\\' : c) );
		}
#if FILE_DEBUG
		fprintf(stderr,"curdir %d -> %s\n",Drive,path);
#endif

		return TRUE;
	}
	else return FALSE;
}

/*-----------------------------------------------------------------------*/
/*
  PExec Load And Go - Redirect to cart' routine at address 0xFA1000
 
  If loading from hard-drive(ie drive ID 2 or more) set condition codes to run own GEMDos routines
*/
static int GemDOS_Pexec_LoadAndGo(Uint32 Params)
{
	/* add multiple disk support here too */
	/* Hard-drive? */
	if (CurrentDrive == emudrives[0]->hd_letter)
	{
		/* If not using A: or B:, use my own routines to load */
		return CALL_PEXEC_ROUTINE;
	}
	else return FALSE;
}

/*-----------------------------------------------------------------------*/
/*
  PExec Load But Don't Go - Redirect to cart' routine at address 0xFA1000
*/
static int GemDOS_Pexec_LoadDontGo(Uint32 Params)
{
	/* Hard-drive? */
	if (CurrentDrive == emudrives[0]->hd_letter)
	{
		return CALL_PEXEC_ROUTINE;
	}
	else return FALSE;
}

/*-----------------------------------------------------------------------*/
/*
  GEMDOS PExec handler
  Call 0x4B
*/
static int GemDOS_Pexec(Uint32 Params)
{
	Uint16 Mode;

	/* Find PExec mode */
	Mode = STMemory_ReadWord(Params+SIZE_WORD);

	/* Re-direct as needed */
	switch(Mode)
	{
	 case 0:      /* Load and go */
		return GemDOS_Pexec_LoadAndGo(Params);
	 case 3:      /* Load, don't go */
		return GemDOS_Pexec_LoadDontGo(Params);
	 case 4:      /* Just go */
		return FALSE;
	 case 5:      /* Create basepage */
		return FALSE;
	 case 6:
		return FALSE;
	}

	/* Default: Still re-direct to TOS */
	return FALSE;
}


/*-----------------------------------------------------------------------*/
/*
  GEMDOS Search Next
  Call 0x4F
*/
static BOOL GemDOS_SNext(Uint32 Params)
{
	struct dirent **temp;
	int Index;

	/* Refresh pDTA pointer (from the current basepage) */
	pDTA = (DTA *)STRAM_ADDR(STMemory_ReadLong(STMemory_ReadLong(act_pd)+32));

	/* Was DTA ours or TOS? */
	if (do_get_mem_long(pDTA->magic) == DTA_MAGIC_NUMBER)
	{

		/* Find index into our list of structures */
		Index = do_get_mem_word(pDTA->index) & (MAX_DTAS_FILES-1);

		if (InternalDTAs[Index].centry >= InternalDTAs[Index].nentries)
		{
			Regs[REG_D0] = GEMDOS_ENMFIL;    /* No more files */
			return TRUE;
		}

		temp = InternalDTAs[Index].found;
		if (PopulateDTA(InternalDTAs[Index].path, temp[InternalDTAs[Index].centry++]) == FALSE)
		{
			fprintf(stderr,"\tError setting DTA.\n");
			return TRUE;
		}

		Regs[REG_D0] = GEMDOS_EOK;
		return TRUE;
	}

	return FALSE;
}


/*-----------------------------------------------------------------------*/
/*
  GEMDOS Find first file
  Call 0x4E
*/
static BOOL GemDOS_SFirst(Uint32 Params)
{
	char szActualFileName[MAX_PATH];
	char tempstr[MAX_PATH];
	char *pszFileName;
	struct dirent **files;
	Uint16 Attr;
	int Drive;
	DIR *fsdir;
	int i,j,count;

	/* Find filename to search for */
	pszFileName = (char *)STRAM_ADDR(STMemory_ReadLong(Params+SIZE_WORD));
	Attr = STMemory_ReadWord(Params+SIZE_WORD+SIZE_LONG);

	/* Refresh pDTA pointer (from the current basepage) */
	pDTA = (DTA *)STRAM_ADDR(STMemory_ReadLong(STMemory_ReadLong(act_pd)+32));

	Drive = GemDOS_IsFileNameAHardDrive(pszFileName);
	if (ISHARDDRIVE(Drive))
	{

		/* And convert to hard drive filename */
		GemDOS_CreateHardDriveFileName(Drive,pszFileName,szActualFileName);

		/* Populate DTA, set index for our use */
		do_put_mem_word(pDTA->index, DTAIndex);
		/* set our dta magic num */
		do_put_mem_long(pDTA->magic, DTA_MAGIC_NUMBER);

		if (InternalDTAs[DTAIndex].bUsed == TRUE)
			ClearInternalDTA();
		InternalDTAs[DTAIndex].bUsed = TRUE;

		/* Were we looking for the volume label? */
		if (Attr == GEMDOS_FILE_ATTRIB_VOLUME_LABEL)
		{
			/* Volume name */
			strcpy(pDTA->dta_name,"EMULATED.001");
			Regs[REG_D0] = GEMDOS_EOK;          /* Got volume */
			return TRUE;
		}

		/* open directory */
		fsfirst_dirname(szActualFileName, InternalDTAs[DTAIndex].path);
		fsdir = opendir(InternalDTAs[DTAIndex].path);

		if (fsdir == NULL)
		{
			Regs[REG_D0] = GEMDOS_EPTHNF;        /* Path not found */
			return TRUE;
		}
		/* close directory */
		closedir(fsdir);

		count = scandir(InternalDTAs[DTAIndex].path, &files, 0, alphasort);
		/* File (directory actually) not found */
		if (count < 0)
		{
			Regs[REG_D0] = GEMDOS_EFILNF;
			return TRUE;
		}

		InternalDTAs[DTAIndex].centry = 0;          /* current entry is 0 */
		fsfirst_dirmask(szActualFileName, tempstr); /* get directory mask */
		InternalDTAs[DTAIndex].found = files;       /* get files */

		/* count & copy the entries that match our mask and discard the rest */
		j = 0;
		for (i=0; i < count; i++)
			if (match(tempstr, files[i]->d_name))
			{
				InternalDTAs[DTAIndex].found[j] = files[i];
				j++;
			}
			else
			{
				free(files[i]);
			}
		InternalDTAs[DTAIndex].nentries = j; /* set number of legal entries */

		/* No files of that match, return error code */
		if (j==0)
		{
			free(files);
			InternalDTAs[DTAIndex].found = NULL;
			Regs[REG_D0] = GEMDOS_EFILNF;        /* File not found */
			return TRUE;
		}

		/* Scan for first file (SNext uses no parameters) */
		GemDOS_SNext(0);
		/* increment DTA index */
		DTAIndex++;
		DTAIndex&=(MAX_DTAS_FILES-1);

		return TRUE;
	}
	return FALSE;
}


/*-----------------------------------------------------------------------*/
/*
  GEMDOS Rename
  Call 0x56
*/
static BOOL GemDOS_Rename(Uint32 Params)
{
	char *pszNewFileName,*pszOldFileName;
	char szNewActualFileName[MAX_PATH],szOldActualFileName[MAX_PATH];
	int NewDrive, OldDrive;

	/* Read details from stack */
	pszOldFileName = (char *)STRAM_ADDR(STMemory_ReadLong(Params+SIZE_WORD+SIZE_WORD));
	pszNewFileName = (char *)STRAM_ADDR(STMemory_ReadLong(Params+SIZE_WORD+SIZE_WORD+SIZE_LONG));

	NewDrive = GemDOS_IsFileNameAHardDrive(pszNewFileName);
	OldDrive = GemDOS_IsFileNameAHardDrive(pszOldFileName);
	if (ISHARDDRIVE(NewDrive) && ISHARDDRIVE(OldDrive))
	{
		/* And convert to hard drive filenames */
		GemDOS_CreateHardDriveFileName(NewDrive,pszNewFileName,szNewActualFileName);
		GemDOS_CreateHardDriveFileName(OldDrive,pszOldFileName,szOldActualFileName);

		/* Rename files */
		if ( rename(szOldActualFileName,szNewActualFileName)==0 )
			Regs[REG_D0] = GEMDOS_EOK;
		else
			Regs[REG_D0] = GEMDOS_EACCDN;        /* Access denied */
		return TRUE;
	}

	return FALSE;
}

/*-----------------------------------------------------------------------*/
/*
  GEMDOS GSDToF
  Call 0x57
*/
static BOOL GemDOS_GSDToF(Uint32 Params)
{
	DATETIME DateTime;
	Uint32 pBuffer;
	int Handle,Flag;

	/* Read details from stack */
	pBuffer = STMemory_ReadLong(Params+SIZE_WORD);
	Handle = STMemory_ReadWord(Params+SIZE_WORD+SIZE_LONG)-BASE_FILEHANDLE;
	Flag = STMemory_ReadWord(Params+SIZE_WORD+SIZE_WORD+SIZE_LONG);

	/* Check handle was valid */
	if (GemDOS_IsInvalidFileHandle(Handle))
	{
		/* No assume was TOS */
		return FALSE;
	}

	/* Set time/date stamp? Do nothing. */
	if (Flag == 1)
	{
		Regs[REG_D0] = GEMDOS_EOK;
		return TRUE;
	}

	Regs[REG_D0] = GEMDOS_ERROR;  /* Invalid parameter */

	if (GemDOS_GetFileInformation(FileHandles[Handle].szActualName, &DateTime) == TRUE)
	{
		STMemory_WriteWord(pBuffer, DateTime.word1);
		STMemory_WriteWord(pBuffer+2, DateTime.word2);
		Regs[REG_D0] = GEMDOS_EOK;
	}
	return TRUE;
}


/*-----------------------------------------------------------------------*/
/*
  Run GEMDos call, and re-direct if need to. Used to handle hard disk emulation etc...
  This sets the condition codes (in SR), which are used in the 'cart_asm.s' program to
  decide if we need to run old GEM vector, or PExec or nothing.
 
  This method keeps the stack and other states consistant with the original ST which is very important
  for the PExec call and maximum compatibility through-out
*/
void GemDOS_OpCode(void)
{
	unsigned short int GemDOSCall,CallingSReg;
	Uint32 Params;
	short RunOld;


	/* Read SReg from stack to see if parameters are on User or Super stack  */
	MakeSR();  /* update value of SR */
	CallingSReg = STMemory_ReadWord(Regs[REG_A7]);
	if ((CallingSReg&SR_SUPERMODE)==0)      /* Calling from user mode */
		Params = regs.usp;
	else
	{
		Params = Regs[REG_A7]+SIZE_WORD+SIZE_LONG;  /* super stack */
		if (cpu_level > 0)
			Params += SIZE_WORD;   /* Skip extra word whe CPU is >=68010 */
	}

	/* Default to run TOS GemDos (SR_NEG run Gemdos, SR_ZERO already done, SR_OVERFLOW run own 'Pexec' */
	RunOld = TRUE;
	SR &= SR_CLEAR_OVERFLOW;
	SR &= SR_CLEAR_ZERO;
	SR |= SR_NEG;

	/* Find pointer to call parameters */
	GemDOSCall = STMemory_ReadWord(Params);

#ifdef GEMDOS_VERBOSE
	if (GemDOSCall < (sizeof(pszGemDOSNames)/sizeof(pszGemDOSNames[0])))
		fprintf(stderr, "GemDOS 0x%X (%s)\n",GemDOSCall,pszGemDOSNames[GemDOSCall]);
	else
		fprintf(stderr, "GemDOS 0x%X\n",GemDOSCall);
#endif

	/* Intercept call */
	switch(GemDOSCall)
	{
	 /*
	 case 0x3:
		if (GemDOS_Cauxin(Params))
			RunOld = FALSE;
		break;
	 */
	 /*
	 case 0x4:
		if (GemDOS_Cauxout(Params))
			RunOld = FALSE;
		break;
	 */
	 case 0x5:          /* direct printing via GEMDOS */
		if (GemDOS_Cprnout(Params))
			RunOld = FALSE;
		break;
	 case 0xe:
		if (GemDOS_SetDrv(Params))
			RunOld = FALSE;
		break;
	 case 0x11:         /* Printer status  */
		if (GemDOS_Cprnos(Params))
			RunOld = FALSE;
		break;
	 /*
	 case 0x12:
		if (GemDOS_Cauxis(Params))
			RunOld = FALSE;
		break;
	 */
	 /*
	 case 0x13:
		if (GemDOS_Cauxos(Params))
			RunOld = FALSE;
		break;
	 */
	 case 0x1a:
		if (GemDOS_SetDTA(Params))
			RunOld = FALSE;
		break;
	 case 0x36:
		if (GemDOS_DFree(Params))
			RunOld = FALSE;
		break;
	 case 0x39:
		if (GemDOS_MkDir(Params))
			RunOld = FALSE;
		break;
	 case 0x3a:
		if (GemDOS_RmDir(Params))
			RunOld = FALSE;
		break;
	 case 0x3b:
		if (GemDOS_ChDir(Params))
			RunOld = FALSE;
		break;
	 case 0x3c:
		if (GemDOS_Create(Params))
			RunOld = FALSE;
		break;
	 case 0x3d:
		if (GemDOS_Open(Params))
			RunOld = FALSE;
		break;
	 case 0x3e:
		if (GemDOS_Close(Params))
			RunOld = FALSE;
		break;
	 case 0x3f:
		if (GemDOS_Read(Params))
			RunOld = FALSE;
		break;
	 case 0x40:
		if (GemDOS_Write(Params))
			RunOld = FALSE;
		break;
	 case 0x41:
		if (GemDOS_UnLink(Params))
			RunOld = FALSE;
		break;
	 case 0x42:
		if (GemDOS_LSeek(Params))
			RunOld = FALSE;
		break;
	 case 0x43:
		if (GemDOS_Fattrib(Params))
			RunOld = FALSE;
		break;
	 case 0x47:
		if (GemDOS_GetDir(Params))
			RunOld = FALSE;
		break;
	 case 0x4b:
		if (GemDOS_Pexec(Params) == CALL_PEXEC_ROUTINE)
			RunOld = CALL_PEXEC_ROUTINE;
		break;
	 case 0x4e:
		if (GemDOS_SFirst(Params))
			RunOld = FALSE;
		break;
	 case 0x4f:
		if (GemDOS_SNext(Params))
			RunOld = FALSE;
		break;
	 case 0x56:
		if (GemDOS_Rename(Params))
			RunOld = FALSE;
		break;
	 case 0x57:
		if (GemDOS_GSDToF(Params))
			RunOld = FALSE;
		break;
	}

	switch(RunOld)
	{
	 case FALSE:        /* skip over branch to pexec to RTE */
		SR |= SR_ZERO;
		break;
	 case CALL_PEXEC_ROUTINE:   /* branch to pexec, then redirect to old gemdos. */
		SR |= SR_OVERFLOW;
		break;
	}

	MakeFromSR();  /* update the flags from the SR register */
}


/*-----------------------------------------------------------------------*/
/*
  GemDOS_Boot - routine called on the first occurence of the gemdos opcode.
  (this should be in the cartridge bootrom)
  Sets up our gemdos handler (or, if we don't need one, just turn off keyclicks)
 */
void GemDOS_Boot(void)
{
	bInitGemDOS = TRUE;

#ifdef GEMDOS_VERBOSE
	fprintf(stderr, "Gemdos_Boot()\n");
#endif

	/* install our gemdos handler, if -e or --harddrive option used */
	if (GEMDOS_EMU_ON)
	{
		/* Get the address of the p_run variable that points to the actual basepage */
		if (TosVersion == 0x100)
		{
			/* We have to use fix addresses on TOS 1.00 :-( */
			if ((STMemory_ReadWord(TosAddress+28)>>1) == 4)
				act_pd = 0x873c;    /* Spanish TOS is different from others! */
			else
				act_pd = 0x602c;
		}
		else
		{
			act_pd = STMemory_ReadLong(TosAddress + 0x28);
		}

		/* Save old GEMDOS handler adress */
		STMemory_WriteLong(CART_OLDGEMDOS, STMemory_ReadLong(0x0084));
		/* Setup new GEMDOS handler, see "cart_asm.s" */
		STMemory_WriteLong(0x0084, CART_GEMDOS);
	}
}

