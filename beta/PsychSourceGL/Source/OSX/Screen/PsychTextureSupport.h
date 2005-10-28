/*
	PsychToolbox3/Source/OSX/Screen/PsychTextureSupport.h
	
	PLATFORMS:	This is the OS X Core Graphics version.  
				
	AUTHORS:
	Allen Ingling		awi		Allen.Ingling@nyu.edu

	HISTORY:
	3/9/04		awi		Wrote it 
							
	DESCRIPTION:
	
	Psychtoolbox functions for dealing with textures.
        

*/




//include once
#ifndef PSYCH_IS_INCLUDED_PsychTextureSupport
#define PSYCH_IS_INCLUDED_PsychTextureSupport

#include "Screen.h"

void PsychInitWindowRecordTextureFields(PsychWindowRecordType *winRec);
void PsychCreateTextureForWindow(PsychWindowRecordType *win);
void PsychFreeTextureForWindowRecord(PsychWindowRecordType *win);
void PsychBlitTextureToDisplay(PsychWindowRecordType *source, PsychWindowRecordType *target, double *sourceRect, double *targetRect,
                               double rotationAngle, int filterMode, double globalAlpha);

//end include once
#endif


