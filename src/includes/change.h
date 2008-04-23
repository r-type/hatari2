/*
  Hatari - change.h
  
  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
#ifndef HATARI_CHANGE_H
#define HATARI_CHANGE_H

#include "configuration.h"

extern BOOL Change_DoNeedReset(CNF_PARAMS *changed);
extern void Change_CopyChangedParamsToConfiguration(CNF_PARAMS *changed, BOOL bForceReset);
extern const char* Change_SetControlSocket(const char *socketpath);
extern void Change_CheckUpdates(void);

#endif /* HATARI_CHANGE_H */
