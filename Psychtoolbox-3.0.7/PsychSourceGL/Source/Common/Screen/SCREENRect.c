/*
  SCREENRect.c		
  
  AUTHORS:
  
		Allen.Ingling@nyu.edu		awi 
  
  PLATFORMS:
  
		Only OS X for now.    

  HISTORY:
  
		1/14/03  awi		Created.   
 
  
  TO DO:
  

*/


#include "Screen.h"



// If you change the useString then also change the corresponding synopsis string in ScreenSynopsis.c
static char useString[] = "rect=Screen('Rect', windowPointerOrScreenNumber);";
//                                             1
static char synopsisString[] = 
	"Get local rect of window or screen.";
static char seeAlsoString[] = "";	

PsychError SCREENRect(void)  
{
	
	PsychWindowRecordType *windowRecord;
	int screenNumber;
	PsychRectType rect; 
    
	//all sub functions should have these two lines
	PsychPushHelp(useString, synopsisString,seeAlsoString);
	if(PsychIsGiveHelp()){PsychGiveHelp();return(PsychError_none);};
	
	//check for superfluous arguments
	PsychErrorExit(PsychCapNumInputArgs(1));   	//The maximum number of inputs
	PsychErrorExit(PsychRequireNumInputArgs(1));	//Insist that the argument be present.   
	PsychErrorExit(PsychCapNumOutputArgs(1));  	//The maximum number of outputs

	if(PsychIsScreenNumberArg(1)){
		PsychCopyInScreenNumberArg(1, TRUE, &screenNumber);
		PsychGetScreenRect(screenNumber, rect);
		PsychCopyOutRectArg(1, FALSE, rect);
	}else if(PsychIsWindowIndexArg(1)){
	   PsychAllocInWindowRecordArg(1, TRUE, &windowRecord);
	   PsychCopyOutRectArg(1,FALSE, windowRecord->rect);
	}else
		PsychErrorExitMsg(PsychError_user, "Argument was recognized as neither a window index nor a screen pointer");

	return(PsychError_none);
}



