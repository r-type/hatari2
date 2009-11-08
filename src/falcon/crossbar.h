/*
  Hatari - crossbar.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_CROSSBAR_H
#define HATARI_CROSSBAR_H

#define CROSSBAR_SNDCTRL_PLAY         0x01
#define CROSSBAR_SNDCTRL_PLAYLOOP     0x02
#define CROSSBAR_SNDCTRL_RECORD       0x10
#define CROSSBAR_SNDCTRL_RECORDLOOP   0x20

#define CROSSBAR_SNDMODE_16BITSTEREO  0x40
#define CROSSBAR_SNDMODE_MONO         0x80

extern Uint16 nCbar_DmaSoundControl;

/* Called by mfp.c */
extern void Crossbar_GenerateSamples(int nMixBufIdx, int nSamplesToGenerate);

extern void Crossbar_Reset(bool bCold);
extern void Crossbar_MemorySnapShot_Capture(bool bSave);

/* Called by ioMemTabFalcon.c */
extern void Crossbar_FrameStartHigh_ReadByte(void);
extern void Crossbar_FrameStartHigh_WriteByte(void);
extern void Crossbar_FrameStartMed_ReadByte(void);
extern void Crossbar_FrameStartMed_WriteByte(void);
extern void Crossbar_FrameStartLow_ReadByte(void);
extern void Crossbar_FrameStartLow_WriteByte(void);
extern void Crossbar_FrameCountHigh_ReadByte(void);
extern void Crossbar_FrameCountHigh_WriteByte(void);
extern void Crossbar_FrameCountMed_ReadByte(void);
extern void Crossbar_FrameCountMed_WriteByte(void);
extern void Crossbar_FrameCountLow_ReadByte(void);
extern void Crossbar_FrameEndHigh_ReadByte(void);
extern void Crossbar_FrameCountLow_WriteByte(void);
extern void Crossbar_FrameEndHigh_WriteByte(void);
extern void Crossbar_FrameEndMed_ReadByte(void);
extern void Crossbar_FrameEndMed_WriteByte(void);
extern void Crossbar_FrameEndLow_ReadByte(void);
extern void Crossbar_FrameEndLow_WriteByte(void);
extern void Crossbar_BufferInter_ReadWord(void);
extern void Crossbar_BufferInter_WriteWord(void);
extern void Crossbar_DmaCtrlReg_ReadWord(void);
extern void Crossbar_DmaCtrlReg_WriteWord(void);
extern void Crossbar_DmaTrckCtrl_ReadByte(void);
extern void Crossbar_DmaTrckCtrl_WriteByte(void);
extern void Crossbar_SoundModeCtrl_ReadByte(void);
extern void Crossbar_SoundModeCtrl_WriteByte(void);
extern void Crossbar_SrcControler_ReadWord(void);
extern void Crossbar_SrcControler_WriteWord(void);
extern void Crossbar_DstControler_ReadWord(void);
extern void Crossbar_DstControler_WriteWord(void);
extern void Crossbar_FreqDivExt_ReadByte(void);
extern void Crossbar_FreqDivExt_WriteByte(void);
extern void Crossbar_FreqDivInt_ReadByte(void);
extern void Crossbar_FreqDivInt_WriteByte(void);
extern void Crossbar_TrackRecSelect_ReadByte(void);
extern void Crossbar_TrackRecSelect_WriteByte(void);
extern void Crossbar_CodecInput_ReadByte(void);
extern void Crossbar_CodecInput_WriteByte(void);
extern void Crossbar_AdcInput_ReadByte(void);
extern void Crossbar_AdcInput_WriteByte(void);
extern void Crossbar_InputAmp_ReadByte(void);
extern void Crossbar_InputAmp_WriteByte(void);
extern void Crossbar_OutputReduct_ReadWord(void);
extern void Crossbar_OutputReduct_WriteWord(void);
extern void Crossbar_CodecStatus_ReadWord(void);
extern void Crossbar_CodecStatus_WriteWord(void);

/* Called by int.c */
extern void Crossbar_InterruptHandler_ADCXmit(void);
extern void Crossbar_InterruptHandler_DspXmit(void);
extern void Crossbar_InterruptHandler_DmaPlay(void);

/* Called by dsp.c */
void Crossbar_DmaPlayInHandShakeMode(void);


Uint16 microphone_ADC_is_started;

#endif /* HATARI_CROSSBAR_H */
