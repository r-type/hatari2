/*
  Hatari - ioMemTabFalcon.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Table with hardware IO handlers for the Falcon.
*/
const char IoMemTabFalc_fileid[] = "Hatari ioMemTabFalcon.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "fdc.h"
#include "ikbd.h"
#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "joy.h"
#include "mfp.h"
#include "midi.h"
#include "nvram.h"
#include "psg.h"
#include "rs232.h"
#include "rtc.h"
#include "screen.h"
#include "video.h"
#include "blitter.h"
#include "crossbar.h"
#include "falcon/videl.h"
#if ENABLE_DSP_EMU
#include "falcon/dsp.h"
#endif

/**
 * no DSP
 */
void IoMemTabFalcon_DSPnone(void (**readtab)(void), void (**writetab)(void))
{
	int i, offset;
	offset = 0xffa200 - 0xff8000;
	for (i = 0; i < 8; i++) {
		readtab[offset+i] = IoMem_ReadWithoutInterception;
	}
	readtab[offset+2] = IoMem_VoidRead;	/* TODO: why this is needed */
	for (i = 0; i < 8; i++) {
		writetab[offset+i] = IoMem_WriteWithoutInterception;
	}
}

/**
 *  Just a temporary hack - some programs are polling on this register and
 * are expecting the handshake bit (#7) to change after a while...
 */
static void DSP_DummyHostCommand_ReadByte(void)
{
	IoMem[0xffa201] ^= 0x80;
}

/**
 *  Just a temporary hack - some programs are polling on this register and
 * are expecting some bits to change after a while...
 */
static void DSP_DummyInterruptStatus_ReadByte(void)
{
	IoMem[0xffa202] ^= 0xff;
}

/**
 * dummy IO when DSP emulation is not enabled
 */
void IoMemTabFalcon_DSPdummy(void (**readtab)(void), void (**writetab)(void))
{
	int i, offset;
	offset = 0xffa200 - 0xff8000;
	readtab[offset+0] = IoMem_ReadWithoutInterception;
	readtab[offset+1] = DSP_DummyHostCommand_ReadByte;
	readtab[offset+2] = DSP_DummyInterruptStatus_ReadByte;
	for (i = 3; i < 8; i++) {
		readtab[offset+i] = IoMem_ReadWithoutInterception;
	}
	for (i = 0; i < 8; i++) {
		writetab[offset+i] = IoMem_WriteWithoutInterception;
	}
}

#if ENABLE_DSP_EMU
/**
 * enable DSP emulation
 */
void IoMemTabFalcon_DSPemulation(void (**readtab)(void), void (**writetab)(void))
{
	int i, offset;
	offset = 0xffa200 - 0xff8000;
	for (i = 0; i < 8; i++) {
		readtab[offset+i]  = DSP_HandleReadAccess;
		writetab[offset+i] = DSP_HandleWriteAccess;
	}
}
#endif


/**
 * Take into account the Falcon Bus Control register $ff8007.b
	$FFFF8007 Falcon Bus Control
		BIT 6 : F30 Start (0=Cold, 1=Warm) 
		BIT 5 : STe Bus Emulation (0=on)
		BIT 3 : Blitter Flag (0=on, 1=off)
		BIT 2 : Blitter (0=8mhz, 1=16mhz)
		BIT 0 : 68030 (0=8mhz, 1=16mhz)
*/
static void IoMemTabFalcon_BusCtrl_WriteByte(void)
{
	Uint8 busCtrl = IoMem_ReadByte(0xff8007);
	
	/* STE bus emulation ? */
	if ((busCtrl & 0x20) == 0)
		IoMem_Init_FalconInSTEcompatibilityMode();
	else
		IoMem_Init();

	/* 68030 Frequency ? */
	if ((busCtrl & 0x1) == 1)
		nCpuFreqShift = 1;
	else
		nCpuFreqShift = 1;
}


/*-----------------------------------------------------------------------*/
/*
  List of functions to handle read/write hardware interceptions for a Falcon.
  Note: This is not working at all yet!
*/
const INTERCEPT_ACCESS_FUNC IoMemTable_Falcon[] =
{
	{ 0xff8000, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xff8001, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception }, /* Memory configuration */
	{ 0xff8006, SIZE_BYTE, IoMem_ReadWithoutInterception, VIDEL_Monitor_WriteByte },        /* Falcon monitor and memory configuration */
	{ 0xff8007, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMemTabFalcon_BusCtrl_WriteByte }, /* Falcon bus configuration */
	{ 0xff800C, SIZE_WORD, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xff8060, SIZE_LONG, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */

	{ 0xff8200, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xff8201, SIZE_BYTE, IoMem_ReadWithoutInterception, VIDEL_ScreenBase_WriteByte },     /* Video base high byte */
	{ 0xff8202, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xff8203, SIZE_BYTE, IoMem_ReadWithoutInterception, VIDEL_ScreenBase_WriteByte },     /* Video base med byte */
	{ 0xff8204, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xff8205, SIZE_BYTE, Video_ScreenCounter_ReadByte, Video_ScreenCounter_WriteByte },
	{ 0xff8206, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xff8207, SIZE_BYTE, Video_ScreenCounter_ReadByte, Video_ScreenCounter_WriteByte },
	{ 0xff8208, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xff8209, SIZE_BYTE, Video_ScreenCounter_ReadByte, Video_ScreenCounter_WriteByte },
	{ 0xff820a, SIZE_BYTE, Video_Sync_ReadByte, Video_Sync_WriteByte },
	{ 0xff820b, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xff820c, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xff820d, SIZE_BYTE, Video_BaseLow_ReadByte, VIDEL_ScreenBase_WriteByte },            /* Video base low byte */
	{ 0xff820e, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception }, /* Falcon line offset */
	{ 0xff820f, SIZE_BYTE, Video_LineWidth_ReadByte, Video_LineWidth_WriteByte },
	{ 0xff8210, SIZE_WORD, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception }, /* Falcon line width */
	{ 0xff8212, 46, IoMem_VoidRead, IoMem_VoidWrite },                                      /* No bus error here */
	{ 0xff8240, 32, IoMem_ReadWithoutInterception, VIDEL_ColorRegsWrite },                  /* ST color regs */
	{ 0xff8260, SIZE_BYTE, Video_ShifterMode_ReadByte, Video_ShifterMode_WriteByte },
	{ 0xff8261, 3,         IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus errors here */
	{ 0xff8264, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception }, /* Falcon horizontal fine scrolling high ? */
	{ 0xff8265, SIZE_BYTE, Video_HorScroll_Read, Video_HorScroll_Write },                   /* horizontal fine scrolling */
	{ 0xff8266, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_FALC_ShiftModeWriteWord },  /* Falcon shift mode */
	{ 0xff8268, 24,        IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus errors here */

	{ 0xff8280, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_HHC_WriteWord },            /* HHC : Horizontal Hold Counter */
	{ 0xff8282, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_HHT_WriteWord },            /* HHT : Horizontal Hold Timer */
	{ 0xff8284, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_HBB_WriteWord },            /* HBB : Horizontal Border Begin */
	{ 0xff8286, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_HBE_WriteWord },            /* HBE : Horizontal Border End */
	{ 0xff8288, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_HDB_WriteWord },            /* HDB : Horizontal Display Begin */
	{ 0xff828a, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_HDE_WriteWord },            /* HDE : Horizontal Display End */
	{ 0xff828c, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_HSS_WriteWord },            /* HSS : Horizontal SS */
	{ 0xff828e, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_HFS_WriteWord },            /* HFS : Horizontal FS */
	{ 0xff8290, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_HEE_WriteWord },            /* HEE : Horizontal EE */
	{ 0xff8292, 14,        IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus errors here */
	{ 0xff82a0, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_VFC_WriteWord },            /* VFC - Vertical Frequency Counter */
	{ 0xff82a2, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_VFT_WriteWord },            /* VFT - Vertical Frequency Timer */
	{ 0xff82a4, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_VBB_WriteWord },            /* VBB - Vertical Border Begin */
	{ 0xff82a6, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_VBE_WriteWord },            /* VBE - Vertical Border End */
	{ 0xff82a8, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_VDB_WriteWord },            /* VDB - Vertical Display Begin */
	{ 0xff82aa, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_VDE_WriteWord },            /* VDE - Vertical Display End */
	{ 0xff82ac, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_VSS_WriteWord },            /* VSS - Vertical SS */
	{ 0xff82ae, 18,        IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus errors here */
	{ 0xff82c0, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_VCO_WriteWord },            /* VCO - Video control */
	{ 0xff82c2, SIZE_WORD, IoMem_ReadWithoutInterception, VIDEL_VMD_WriteWord },            /* VMD - Video mode */

	{ 0xff8604, SIZE_WORD, FDC_DiskControllerStatus_ReadWord, FDC_DiskController_WriteWord },
	{ 0xff8606, SIZE_WORD, FDC_DmaStatus_ReadWord, FDC_DmaModeControl_WriteWord },
	{ 0xff8608, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xff8609, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception }, /* DMA base and counter high byte */
	{ 0xff860a, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xff860b, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception }, /* DMA base and counter med byte  */
	{ 0xff860c, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xff860d, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception }, /* DMA base and counter low byte  */
	{ 0xff860e, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xff860f, SIZE_BYTE, FDC_FloppyMode_ReadByte, FDC_FloppyMode_WriteByte },             /* Floppy mode (?) register */

	{ 0xff8800, SIZE_BYTE, PSG_ff8800_ReadByte, PSG_ff8800_WriteByte },
	{ 0xff8801, SIZE_BYTE, PSG_ff880x_ReadByte, PSG_ff8801_WriteByte },
	{ 0xff8802, SIZE_BYTE, PSG_ff880x_ReadByte, PSG_ff8802_WriteByte },
	{ 0xff8803, SIZE_BYTE, PSG_ff880x_ReadByte, PSG_ff8803_WriteByte },

	{ 0xff8900, SIZE_BYTE, IoMem_ReadWithoutInterception, Crossbar_BufferInter_WriteByte },       /* Crossbar Buffer interrupts */
	{ 0xff8901, SIZE_BYTE, IoMem_ReadWithoutInterception, Crossbar_DmaCtrlReg_WriteByte },        /* Crossbar control register */
	{ 0xff8902, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                                     /* No bus error here */
	{ 0xff8903, SIZE_BYTE, Crossbar_FrameStartHigh_ReadByte, Crossbar_FrameStartHigh_WriteByte }, /* DMA sound frame start high */
	{ 0xff8904, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                                     /* No bus error here */
	{ 0xff8905, SIZE_BYTE, Crossbar_FrameStartMed_ReadByte, Crossbar_FrameStartMed_WriteByte },   /* DMA sound frame start med */
	{ 0xff8906, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                                     /* No bus error here */
	{ 0xff8907, SIZE_BYTE, Crossbar_FrameStartLow_ReadByte, Crossbar_FrameStartLow_WriteByte },   /* DMA sound frame start low */
	{ 0xff8908, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                                     /* No bus error here */
	{ 0xff8909, SIZE_BYTE, Crossbar_FrameCountHigh_ReadByte, Crossbar_FrameCountHigh_WriteByte }, /* DMA sound frame count high */
	{ 0xff890a, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                                     /* No bus error here */
	{ 0xff890b, SIZE_BYTE, Crossbar_FrameCountMed_ReadByte, Crossbar_FrameCountMed_WriteByte },   /* DMA sound frame count med */
	{ 0xff890c, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                                     /* No bus error here */
	{ 0xff890d, SIZE_BYTE, Crossbar_FrameCountLow_ReadByte, Crossbar_FrameCountLow_WriteByte },   /* DMA sound frame count low */
	{ 0xff890e, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                                     /* No bus error here */
	{ 0xff890f, SIZE_BYTE, Crossbar_FrameEndHigh_ReadByte, Crossbar_FrameEndHigh_WriteByte },     /* DMA sound frame end high */
	{ 0xff8910, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                                     /* No bus error here */
	{ 0xff8911, SIZE_BYTE, Crossbar_FrameEndMed_ReadByte, Crossbar_FrameEndMed_WriteByte },       /* DMA sound frame end med */
	{ 0xff8912, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                                     /* No bus error here */
	{ 0xff8913, SIZE_BYTE, Crossbar_FrameEndLow_ReadByte, Crossbar_FrameEndLow_WriteByte },       /* DMA sound frame end low */
	{ 0xff8920, SIZE_BYTE, IoMem_ReadWithoutInterception, Crossbar_DmaTrckCtrl_WriteByte },       /* Crossbar track control */
	{ 0xff8921, SIZE_BYTE, IoMem_ReadWithoutInterception, Crossbar_SoundModeCtrl_WriteByte },     /* DMA sound mode control */
	{ 0xff8922, SIZE_WORD, IoMem_VoidRead, IoMem_VoidWrite },                                     /* Microwire data */
	{ 0xff8924, SIZE_WORD, IoMem_VoidRead, IoMem_VoidWrite },                                     /* Microwire mask */

	{ 0xff8930, SIZE_WORD, IoMem_ReadWithoutInterception, Crossbar_SrcControler_WriteWord },   /* Crossbar source controler */
	{ 0xff8932, SIZE_WORD, IoMem_ReadWithoutInterception, Crossbar_DstControler_WriteWord },   /* Crossbar destination controler */
	{ 0xff8934, SIZE_BYTE, IoMem_ReadWithoutInterception, Crossbar_FreqDivExt_WriteByte },     /* External clock divider */
	{ 0xff8935, SIZE_BYTE, IoMem_ReadWithoutInterception, Crossbar_FreqDivInt_WriteByte },     /* Internal clock divider */
	{ 0xff8936, SIZE_BYTE, IoMem_ReadWithoutInterception, Crossbar_TrackRecSelect_WriteByte }, /* Track record select */
	{ 0xff8937, SIZE_BYTE, IoMem_ReadWithoutInterception, Crossbar_CodecInput_WriteByte },     /* CODEC input source from 16 bits adder */
	{ 0xff8938, SIZE_BYTE, IoMem_ReadWithoutInterception, Crossbar_AdcInput_WriteByte },       /* ADC converter input for L+R channel */
	{ 0xff8939, SIZE_BYTE, IoMem_ReadWithoutInterception, Crossbar_InputAmp_WriteByte },       /* Input amplifier (+1.5 dB step) */
	{ 0xff893a, SIZE_BYTE, IoMem_ReadWithoutInterception, Crossbar_OutputReduct_WriteByte },   /* Output reduction (-1.5 dB step) */
	{ 0xff893b, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },    /* No bus error here */
	{ 0xff893c, SIZE_WORD, IoMem_ReadWithoutInterception, Crossbar_CodecStatus_WriteWord },    /* CODEC status */
	{ 0xff893e, SIZE_WORD, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },    /* No bus error here */
	{ 0xff8940, SIZE_WORD, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },    /* GPx direction */
	{ 0xff8942, SIZE_WORD, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },    /* GPx port */

	{ 0xff8960, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xff8961, SIZE_BYTE, NvRam_Select_ReadByte, NvRam_Select_WriteByte },                 /* NVRAM/RTC chip */
	{ 0xff8962, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xff8963, SIZE_BYTE, NvRam_Data_ReadByte, NvRam_Data_WriteByte },                     /* NVRAM/RTC chip */

	{ 0xff8a00, SIZE_WORD, Blitter_Halftone00_ReadWord, Blitter_Halftone00_WriteWord }, /* Blitter halftone RAM 0 */
	{ 0xff8a02, SIZE_WORD, Blitter_Halftone01_ReadWord, Blitter_Halftone01_WriteWord }, /* Blitter halftone RAM 1 */
	{ 0xff8a04, SIZE_WORD, Blitter_Halftone02_ReadWord, Blitter_Halftone02_WriteWord }, /* Blitter halftone RAM 2 */
	{ 0xff8a06, SIZE_WORD, Blitter_Halftone03_ReadWord, Blitter_Halftone03_WriteWord }, /* Blitter halftone RAM 3 */
	{ 0xff8a08, SIZE_WORD, Blitter_Halftone04_ReadWord, Blitter_Halftone04_WriteWord }, /* Blitter halftone RAM 4 */
	{ 0xff8a0a, SIZE_WORD, Blitter_Halftone05_ReadWord, Blitter_Halftone05_WriteWord }, /* Blitter halftone RAM 5 */
	{ 0xff8a0c, SIZE_WORD, Blitter_Halftone06_ReadWord, Blitter_Halftone06_WriteWord }, /* Blitter halftone RAM 6 */
	{ 0xff8a0e, SIZE_WORD, Blitter_Halftone07_ReadWord, Blitter_Halftone07_WriteWord }, /* Blitter halftone RAM 7 */
	{ 0xff8a10, SIZE_WORD, Blitter_Halftone08_ReadWord, Blitter_Halftone08_WriteWord }, /* Blitter halftone RAM 8 */
	{ 0xff8a12, SIZE_WORD, Blitter_Halftone09_ReadWord, Blitter_Halftone09_WriteWord }, /* Blitter halftone RAM 9 */
	{ 0xff8a14, SIZE_WORD, Blitter_Halftone10_ReadWord, Blitter_Halftone10_WriteWord }, /* Blitter halftone RAM 10 */
	{ 0xff8a16, SIZE_WORD, Blitter_Halftone11_ReadWord, Blitter_Halftone11_WriteWord }, /* Blitter halftone RAM 11 */
	{ 0xff8a18, SIZE_WORD, Blitter_Halftone12_ReadWord, Blitter_Halftone12_WriteWord }, /* Blitter halftone RAM 12 */
	{ 0xff8a1a, SIZE_WORD, Blitter_Halftone13_ReadWord, Blitter_Halftone13_WriteWord }, /* Blitter halftone RAM 13 */
	{ 0xff8a1c, SIZE_WORD, Blitter_Halftone14_ReadWord, Blitter_Halftone14_WriteWord }, /* Blitter halftone RAM 14 */
	{ 0xff8a1e, SIZE_WORD, Blitter_Halftone15_ReadWord, Blitter_Halftone15_WriteWord }, /* Blitter halftone RAM 15 */
	{ 0xff8a20, SIZE_WORD, Blitter_SourceXInc_ReadWord, Blitter_SourceXInc_WriteWord }, /* Blitter source x increment */
	{ 0xff8a22, SIZE_WORD, Blitter_SourceYInc_ReadWord, Blitter_SourceYInc_WriteWord }, /* Blitter source y increment */
	{ 0xff8a24, SIZE_LONG, Blitter_SourceAddr_ReadLong, Blitter_SourceAddr_WriteLong },     /* Blitter source address */
	{ 0xff8a28, SIZE_WORD, Blitter_Endmask1_ReadWord, Blitter_Endmask1_WriteWord },
	{ 0xff8a2a, SIZE_WORD, Blitter_Endmask2_ReadWord, Blitter_Endmask2_WriteWord },
	{ 0xff8a2c, SIZE_WORD, Blitter_Endmask3_ReadWord, Blitter_Endmask3_WriteWord },
	{ 0xff8a2e, SIZE_WORD, Blitter_DestXInc_ReadWord, Blitter_DestXInc_WriteWord }, /* Blitter dest. x increment */
	{ 0xff8a30, SIZE_WORD, Blitter_DestYInc_ReadWord, Blitter_DestYInc_WriteWord }, /* Blitter dest. y increment */
	{ 0xff8a32, SIZE_LONG, Blitter_DestAddr_ReadLong, Blitter_DestAddr_WriteLong },
	{ 0xff8a36, SIZE_WORD, Blitter_WordsPerLine_ReadWord, Blitter_WordsPerLine_WriteWord },
	{ 0xff8a38, SIZE_WORD, Blitter_LinesPerBitblock_ReadWord, Blitter_LinesPerBitblock_WriteWord },
	{ 0xff8a3a, SIZE_BYTE, Blitter_HalftoneOp_ReadByte, Blitter_HalftoneOp_WriteByte },
	{ 0xff8a3b, SIZE_BYTE, Blitter_LogOp_ReadByte, Blitter_LogOp_WriteByte },
	{ 0xff8a3c, SIZE_BYTE, Blitter_Control_ReadByte, Blitter_Control_WriteByte },
	{ 0xff8a3d, SIZE_BYTE, Blitter_Skew_ReadByte, Blitter_Skew_WriteByte },
	{ 0xff8a3e, SIZE_WORD, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */

	{ 0xff8c80, 8, IoMem_VoidRead, IoMem_WriteWithoutInterception },                        /* TODO: SCC */

	{ 0xff9200, SIZE_WORD, Joy_StePadButtons_ReadWord, IoMem_WriteWithoutInterception }, /* Joypad fire buttons */
	{ 0xff9202, SIZE_WORD, Joy_StePadMulti_ReadWord, Joy_StePadMulti_WriteWord },     /* Joypad directions/buttons/selection */
	{ 0xff9206, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                         /* No bus error here ; fix Wotanoid game */
	{ 0xff9210, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                         /* No bus error here */
	{ 0xff9211, SIZE_BYTE, IoMem_VoidRead, IoMem_WriteWithoutInterception },          /* Joypad 0 X position (?) */
	{ 0xff9212, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                         /* No bus error here */
	{ 0xff9213, SIZE_BYTE, IoMem_VoidRead, IoMem_WriteWithoutInterception },          /* Joypad 0 Y position (?) */
	{ 0xff9214, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                         /* No bus error here */
	{ 0xff9215, SIZE_BYTE, IoMem_VoidRead, IoMem_WriteWithoutInterception },          /* Joypad 1 X position (?) */
	{ 0xff9216, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                         /* No bus error here */
	{ 0xff9217, SIZE_BYTE, IoMem_VoidRead, IoMem_WriteWithoutInterception },          /* Joypad 1 Y position (?) */
	{ 0xff9220, SIZE_WORD, IoMem_VoidRead, IoMem_WriteWithoutInterception },          /* Lightpen X position */
	{ 0xff9222, SIZE_WORD, IoMem_VoidRead, IoMem_WriteWithoutInterception },          /* Lightpen Y position */

	{ 0xff9800, 0x400, IoMem_ReadWithoutInterception, VIDEL_ColorRegsWrite },            /* Falcon Videl palette */

	{ 0xfffa00, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa01, SIZE_BYTE, MFP_GPIP_ReadByte, MFP_GPIP_WriteByte },
	{ 0xfffa02, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa03, SIZE_BYTE, MFP_ActiveEdge_ReadByte, MFP_ActiveEdge_WriteByte },
	{ 0xfffa04, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa05, SIZE_BYTE, MFP_DataDirection_ReadByte, MFP_DataDirection_WriteByte },
	{ 0xfffa06, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa07, SIZE_BYTE, MFP_EnableA_ReadByte, MFP_EnableA_WriteByte },
	{ 0xfffa08, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa09, SIZE_BYTE, MFP_EnableB_ReadByte, MFP_EnableB_WriteByte },
	{ 0xfffa0a, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa0b, SIZE_BYTE, MFP_PendingA_ReadByte, MFP_PendingA_WriteByte },
	{ 0xfffa0c, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa0d, SIZE_BYTE, MFP_PendingB_ReadByte, MFP_PendingB_WriteByte },
	{ 0xfffa0e, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa0f, SIZE_BYTE, MFP_InServiceA_ReadByte, MFP_InServiceA_WriteByte },
	{ 0xfffa10, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa11, SIZE_BYTE, MFP_InServiceB_ReadByte, MFP_InServiceB_WriteByte },
	{ 0xfffa12, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa13, SIZE_BYTE, MFP_MaskA_ReadByte, MFP_MaskA_WriteByte },
	{ 0xfffa14, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa15, SIZE_BYTE, MFP_MaskB_ReadByte, MFP_MaskB_WriteByte },
	{ 0xfffa16, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa17, SIZE_BYTE, MFP_VectorReg_ReadByte, MFP_VectorReg_WriteByte },
	{ 0xfffa18, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa19, SIZE_BYTE, MFP_TimerACtrl_ReadByte, MFP_TimerACtrl_WriteByte },
	{ 0xfffa1a, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa1b, SIZE_BYTE, MFP_TimerBCtrl_ReadByte, MFP_TimerBCtrl_WriteByte },
	{ 0xfffa1c, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa1d, SIZE_BYTE, MFP_TimerCDCtrl_ReadByte, MFP_TimerCDCtrl_WriteByte },
	{ 0xfffa1e, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa1f, SIZE_BYTE, MFP_TimerAData_ReadByte, MFP_TimerAData_WriteByte },
	{ 0xfffa20, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa21, SIZE_BYTE, MFP_TimerBData_ReadByte, MFP_TimerBData_WriteByte },
	{ 0xfffa22, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa23, SIZE_BYTE, MFP_TimerCData_ReadByte, MFP_TimerCData_WriteByte },
	{ 0xfffa24, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa25, SIZE_BYTE, MFP_TimerDData_ReadByte, MFP_TimerDData_WriteByte },

	{ 0xfffa26, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa27, SIZE_BYTE, RS232_SCR_ReadByte, RS232_SCR_WriteByte },                 /* Sync character register */
	{ 0xfffa28, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa29, SIZE_BYTE, RS232_UCR_ReadByte, RS232_UCR_WriteByte },                  /* USART control register */
	{ 0xfffa2a, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa2b, SIZE_BYTE, RS232_RSR_ReadByte, RS232_RSR_WriteByte },                /* Receiver status register */
	{ 0xfffa2c, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa2d, SIZE_BYTE, RS232_TSR_ReadByte, RS232_TSR_WriteByte },             /* Transmitter status register */
	{ 0xfffa2e, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffa2f, SIZE_BYTE, RS232_UDR_ReadByte, RS232_UDR_WriteByte },                     /* USART data register */

	{ 0xfffc00, SIZE_BYTE, IKBD_KeyboardControl_ReadByte, IKBD_KeyboardControl_WriteByte },
	{ 0xfffc01, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffc02, SIZE_BYTE, IKBD_KeyboardData_ReadByte, IKBD_KeyboardData_WriteByte },
	{ 0xfffc03, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffc04, SIZE_BYTE, Midi_Control_ReadByte, Midi_Control_WriteByte },
	{ 0xfffc05, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus error here */
	{ 0xfffc06, SIZE_BYTE, Midi_Data_ReadByte, Midi_Data_WriteByte },
	{ 0xfffc07, SIZE_BYTE, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus errors here */

	{ 0xffff82, SIZE_WORD, IoMem_VoidRead, IoMem_VoidWrite },                               /* No bus errors here */

	{ 0, 0, NULL, NULL }
};
