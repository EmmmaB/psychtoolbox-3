/*

	SCREENOpenWindow.c		

  

	AUTHORS:



		Allen.Ingling@nyu.edu		awi 

  

	PLATFORMS:	All

    



	HISTORY:



		12/18/01	awi		Created.  Copied the Synopsis string from old version of psychtoolbox. 

		10/18/02	awi		Added defaults to allow for optional arguments.

		12/05/02	awi		Started over again for OS X without SDL.

		10/12/04	awi		In useString: changed "SCREEN" to "Screen", and moved commas to inside [].

                2/15/05         awi             Commented out glEnable(GL_BLEND) and mode settings.  

                04/03/05        mk              Added support for selecting binocular stereo output via native OpenGL.

	TO DO:

  



*/





#include "Screen.h"





// If you change the useString then also change the corresponding synopsis string in ScreenSynopsis.c

static char useString[] =  "[windowPtr,rect]=Screen('OpenWindow',windowPtrOrScreenNumber [,color] [,rect][,pixelSize][,numberOfBuffers][,stereomode]);";

//                                                               1                         2        3      4           5                 6

static char synopsisString[] =

	"Open an onscreen window. Specify a screen by a windowPtr or a screenNumber (0 is "

	"the main screen, with menu bar). \"color\" is the clut index (scalar or [r g b] "

	"triplet) that you want to poke into each pixel; default is white. If supplied, "

	"\"rect\" must contain at least one pixel. If a windowPtr is supplied then \"rect\" "

	"is in the window's coordinates (origin at upper left), and defaults to the whole "

	"window. If a screenNumber is supplied then \"rect\" is in screen coordinates "

	"(origin at upper left), and defaults to the whole screen. (In all cases, "

	"subsequent references to this new window will use its coordinates: origin at its "

	"upper left.) The Windows and OS-X version accepts \"rect\" but disregards it, the window is "

	"always the size of the display on which it appears. \"pixelSize\" sets the depth "

	"(in bits) of each pixel; default is to leave depth unchanged. "

        "\"numberOfBuffers\" is the number of buffers to use. Setting anything else than 2 will be "

        "useful for development/debugging of PTB itself but will mess up any real experiment. "

        "\"stereomode\" Type of stereo display algorithm to use: 0 (default) means: Monoscopic viewing. "

        "1 means: Stereo output via OpenGL on any stereo hardware that is supported by MacOS-X, e.g., the "

        "shutter glasses from CrystalView. "

        "Opening or closing a window takes about two to three seconds, depending on type of connected display. ";  

static char seeAlsoString[] = "OpenOffscreenWindow, SelectStereoDrawBuffer";

	



PsychError SCREENOpenWindow(void) 

{

    int					screenNumber, numWindowBuffers, stereomode;

    PsychRectType 			rect;

    PsychColorType			color;

    PsychColorModeType  		mode; 

    boolean				isArgThere, settingsMade, didWindowOpen;

    PsychScreenSettingsType		screenSettings;

    PsychWindowRecordType		*windowRecord;

    

    PsychDepthType		specifiedDepth, possibleDepths, currentDepth, useDepth;

    

    //just for debugging

    //printf("Entering SCREENOpen\n");

    

    //all sub functions should have these two lines

    PsychPushHelp(useString, synopsisString, seeAlsoString);

    if(PsychIsGiveHelp()){PsychGiveHelp();return(PsychError_none);};

    

    //cap the number of inputs

    PsychErrorExit(PsychCapNumInputArgs(6));   //The maximum number of inputs

    PsychErrorExit(PsychCapNumOutputArgs(2));  //The maximum number of outputs

    

    //get the screen number from the windowPtrOrScreenNumber.  This also checks to make sure that the specified screen exists.  

    PsychCopyInScreenNumberArg(kPsychUseDefaultArgPosition, TRUE, &screenNumber);

    if(screenNumber==-1)

        PsychErrorExitMsg(PsychError_user, "The specified offscreen window has no ancestral screen."); 

    

    /*

        The depth checking is ugly because of this stupid depth structure stuff.  

     Instead get a descriptor of the current video settings, change the depth field,

     and pass it to a validate function wich searches a list of valid video modes for the display.

     

     There seems to be no point in checking the depths alone because the legality of a particular

     depth depends on the other settings specified below.  Its probably best to wait until we have

     digested all settings and then test the full mode, declarin an invalid

     mode and not an invalid pixel size.  We could notice when the depth alone is specified 

     and in that case issue an invalid depth value.

     */  

    //find the PixelSize first because the color specifier depends on the screen depth.  

    PsychInitDepthStruct(&currentDepth);  //get the current depth

    PsychGetScreenDepth(screenNumber, &currentDepth);

    PsychInitDepthStruct(&possibleDepths); //get the possible depths

    PsychGetScreenDepths(screenNumber, &possibleDepths);

    PsychInitDepthStruct(&specifiedDepth); //get the requested depth and validate it.  

    isArgThere = PsychCopyInSingleDepthArg(4, FALSE, &specifiedDepth);

    PsychInitDepthStruct(&useDepth);

    if(isArgThere){ //if the argument is there check that the screen supports it...

        if(!PsychIsMemberDepthStruct(&specifiedDepth, &possibleDepths))

            PsychErrorExit(PsychError_invalidDepthArg);

        else

            PsychCopyDepthStruct(&useDepth, &specifiedDepth);

    }else //otherwise use the default

        PsychCopyDepthStruct(&useDepth, &currentDepth);

    

    //find the color.  We do this here because the validity of this argument depends on the depth.

    isArgThere=PsychCopyInColorArg(kPsychUseDefaultArgPosition, FALSE, &color); //get from user

    if(!isArgThere)

        PsychLoadColorStruct(&color, kPsychIndexColor, PsychGetWhiteValueFromDepthStruct(&useDepth)); //or use the default

    mode=PsychGetColorModeFromDepthStruct(&useDepth);

    PsychCoerceColorMode(mode, &color);  //transparent if mode match, error exit if invalid conversion.

    

    //find the rect.

    PsychGetScreenRect(screenNumber, rect); 	//get the rect describing the screen bounds.  This is the default Rect.  

    if(!kPsychAllWindowsFull)

        isArgThere=PsychCopyInRectArg(kPsychUseDefaultArgPosition, FALSE, rect );

    

    //find the number of specified buffers. 

    //OS X:	The number of backbuffers is not a property of the display mode but an attribute of the pixel format.

    //		Therefore the value is held by a window record and not a screen record.    

    numWindowBuffers=2;	

    PsychCopyInIntegerArg(5,FALSE,&numWindowBuffers);

    if(numWindowBuffers < 1 || numWindowBuffers > kPsychMaxNumberWindowBuffers)

        PsychErrorExit(PsychError_invalidNumberBuffersArg);

    

    // MK: Check for optional spec of stereoscopic display: 0 (the default) = monoscopic viewing.
    // 1 == Stereo output via OpenGL built-in stereo facilities: This will drive any kind of
    // stereo display hardware that is directly supported by MacOS-X.
    // 2/3 == Stereo output via compressed frame output: Only one backbuffer is used for both
    // views: The left view image is put into the top-half of the screen, the right view image
    // is put into the bottom half of the screen. External hardware demangles this combi-image
    // again into two separate images. CrystalEyes seems to be able to do this. One looses half
    // of the vertical resolution, but potentially gains refresh rate...
    // Future PTB version may include different stereo algorithms with an id > 1, e.g., 

    // anaglyph stereo, interlaced stereo, ...

    stereomode=0;

    PsychCopyInIntegerArg(6,FALSE,&stereomode);

    if(stereomode < 0 || stereomode > 1) PsychErrorExitMsg(PsychError_user, "Invalid stereomode provided (Valid between 0 and 9).");

    

    //set the video mode to change the pixel size.  TO DO: Set the rect and the default color  

    PsychGetScreenSettings(screenNumber, &screenSettings);    

    PsychInitDepthStruct(&(screenSettings.depth));

    PsychCopyDepthStruct(&(screenSettings.depth), &useDepth);

    

    //Here is where all the work goes on

    //if the screen is not already captured then to that

    if(~PsychIsScreenCaptured(screenNumber)){

        PsychCaptureScreen(screenNumber);

        settingsMade=PsychSetScreenSettings(screenNumber, &screenSettings); 

        //Capturing the screen and setting its settings always occur in conjunction.

        //There should be a check above to see if the display is captured and openWindow is attempting to change

        //the bit depth.

    }

    didWindowOpen=PsychOpenOnscreenWindow(&screenSettings, &windowRecord, numWindowBuffers, stereomode);

    if(!didWindowOpen){

        PsychReleaseScreen(screenNumber);

        // We use this dirty hack to exit with an error, but without printing

        // an error message. The specific error message has been printed in

        // PsychOpenOnscreenWindow() already...

        PsychErrMsgTxt("");

    }

    

    //create the shadow texture for this window

    PsychCreateTextureForWindow(windowRecord);

    

    //set the alpha blending rule   

    PsychSetGLContext(windowRecord); 

    // glEnable(GL_BLEND);

    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //			sFactor		   dFactor



    

    //Return the window index and the rect argument.

    PsychCopyOutDoubleArg(1, FALSE, windowRecord->windowIndex);

    PsychCopyOutRectArg(2, FALSE, rect);

    return(PsychError_none);    

}
