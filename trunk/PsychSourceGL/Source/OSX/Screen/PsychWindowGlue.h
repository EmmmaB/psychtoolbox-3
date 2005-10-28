/*
	PsychToolbox3/Source/OSX/Screen/PsychWindowGlue.h
	
	PLATFORMS:	
	
		This is the OS X Core Graphics version only.  
				
	AUTHORS:
	
		Allen Ingling		awi		Allen.Ingling@nyu.edu

	HISTORY:
	
		12/20/02		awi		Wrote it mostly by modifying SDL-specific refugees (from an experimental SDL-based Psychtoolbox).
		11/16/04		awi		Added description.  
		04/03/05                mk              Support for stereo display output and enhanced Screen('Flip') behaviour.
                05/09/05                mk              New function PsychGetMonitorRefreshInterval -- queries (and measures) monitor refresh.
	
        DESCRIPTION:
	
		Functions in this file comprise an abstraction layer for probing and controlling window state, except for window content.  
		
		Each C function which implements a particular Screen subcommand should be platform neutral.  For example, the source to SCREENPixelSizes() 
		should be platform-neutral, despite that the calls in OS X and Windows to detect available pixel sizes are different.  Platform 
		specificity is abstracted out in C files which end it "Glue", for example PsychScreenGlue, PsychWindowGlue, PsychWindowTextClue.

	NOTES:
	
	TO DO: 
	
		� The "glue" files should should be suffixed with a platform name.  The original (bad) plan was to distingish platform-specific files with the same 
		name by their placement in a directory tree.
		
		� PsychFlushGL() and PsychGLFlush() need to be merged.

        

*/

//include once
#ifndef PSYCH_IS_INCLUDED_PsychWindowGlue
#define PSYCH_IS_INCLUDED_PsychWindowGlue

#include "Screen.h"

boolean PsychOpenOnscreenWindow(PsychScreenSettingsType *screenSettings, PsychWindowRecordType **windowRecord, int numBuffers, int stereomode);
boolean PsychOpenOffscreenWindow(double *rect, int depth, PsychWindowRecordType **windowRecord);
void	PsychCloseOnscreenWindow(PsychWindowRecordType *windowRecord);
void	PsychCloseWindow(PsychWindowRecordType *windowRecord);
void	PsychCloseOffscreenWindow(PsychWindowRecordType *windowRecord);
void	PsychFlushGL(PsychWindowRecordType *windowRecord);
double	PsychFlipWindowBuffers(PsychWindowRecordType *windowRecord, int multiflip, int vbl_synclevel, int dont_clear, double flipwhen, int* beamPosAtFlip, double* miss_estimate, double* time_at_flipend, double* time_at_onset);
void	PsychSetGLContext(PsychWindowRecordType *windowRecord);
void	PsychUnsetGLContext(void);
double  PsychGetMonitorRefreshInterval(PsychWindowRecordType *windowRecord, int* numSamples, double* maxsecs, double* stddev, double intervalHint);
void    PsychVisualBell(PsychWindowRecordType *windowRecord, double duration, int belltype);

//end include once
#endif


