/*
	PsychToolbox3/Source/Common/Screen/SCREENCopyWindow.c		

	AUTHORS:
	
	Allen.Ingling@nyu.edu				awi 
	Mario.Kleiner@tuebingen.mpg.de		mk

	PLATFORMS:	
		
	All.  
    
	HISTORY:
	
	02/19/03	awi		Created.
	03/11/04	awi		Modified for textures
	1/11/05		awi		Cosmetic
	1/14/05		awi		added glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE) at mk's suggestion;
	1/25/05		awi		Relocated glTexEnvf below glBindTexture.  Fix provided by mk.  
	1/22/05		 mk		Completely rewritten for the new OffscreenWindow implementation.
 
	TO DO:

*/

#include "Screen.h"

static char useString[] =  "Screen('CopyWindow',srcWindowPtr,dstWindowPtr,[srcRect],[dstRect],[copyMode])";
static char synopsisString[] =  "Copy images, quickly, between two windows (on- or off- screen). "
                                "srcRect and dstRect are set to the size of windows srcWindowPtr and dstWindowPtr "
                                "by default. [copyMode] is accepted as input but currently ignored. "
                                "CopyWindow is mostly here for compatibility to PTB-2. If you want to "
                                "copy images really quickly, use the 'MakeTexture' and 'DrawTexture' commands."
                                " They also allow for rotated drawing and advanced blending operations. "
								"The current CopyWindow implementation has a couple of restrictions: "
								"One can't copy from an offscreen window into the -same- offscreen window. "
								"One can't copy from an onscreen window into a -different- onscreen window. "
								"Sizes of sourceRect and targetRect need to match for Onscreen->Offscreen copy. ";

static char seeAlsoString[] = "PutImage, GetImage, OpenOffscreenWindow, MakeTexture, DrawTexture";

PsychError SCREENCopyWindow(void) 
{
	PsychRectType		sourceRect, targetRect, targetRectInverted;
	PsychWindowRecordType	*sourceWin, *targetWin;
	GLdouble		sourceVertex[2], targetVertex[2]; 
	double  t1;
	double			sourceRectWidth, sourceRectHeight;
        
	//all sub functions should have these two lines
	PsychPushHelp(useString, synopsisString, seeAlsoString);
	if(PsychIsGiveHelp()){PsychGiveHelp();return(PsychError_none);};

	//cap the number of inputs
	PsychErrorExit(PsychCapNumInputArgs(5));   //The maximum number of inputs
	PsychErrorExit(PsychCapNumOutputArgs(0));  //The maximum number of outputs
        
	//get parameters for the source window:
	PsychAllocInWindowRecordArg(1, TRUE, &sourceWin);
	PsychCopyRect(sourceRect, sourceWin->rect);

	// Special case for stereo: Only half the real window width:
	PsychMakeRect(&sourceRect, sourceWin->rect[kPsychLeft], sourceWin->rect[kPsychTop],
				  sourceWin->rect[kPsychLeft] + PsychGetWidthFromRect(sourceWin->rect)/((sourceWin->specialflags & kPsychHalfWidthWindow) ? 2 : 1),
				  sourceWin->rect[kPsychTop] + PsychGetHeightFromRect(sourceWin->rect)/((sourceWin->specialflags & kPsychHalfHeightWindow) ? 2 : 1));
	
	PsychCopyInRectArg(3, FALSE, sourceRect);
	if (IsPsychRectEmpty(sourceRect)) return(PsychError_none);

	//get paramters for the target window:
	PsychAllocInWindowRecordArg(2, TRUE, &targetWin);

	// By default, the targetRect is equal to the sourceRect, but centered in
	// the target window.
	PsychCopyRect(targetRect, targetWin->rect);

	// Special case for stereo: Only half the real window width:
	PsychMakeRect(&targetRect, targetWin->rect[kPsychLeft], targetWin->rect[kPsychTop],
				  targetWin->rect[kPsychLeft] + PsychGetWidthFromRect(targetWin->rect)/((targetWin->specialflags & kPsychHalfWidthWindow) ? 2 : 1),
				  targetWin->rect[kPsychTop] + PsychGetHeightFromRect(targetWin->rect)/((targetWin->specialflags & kPsychHalfHeightWindow) ? 2 : 1));

	PsychCopyInRectArg(4, FALSE, targetRect);
	if (IsPsychRectEmpty(targetRect)) return(PsychError_none);

	if (0) {
		printf("SourceRect: %f %f %f %f  ---> TargetRect: %f %f %f %f\n", sourceRect[0], sourceRect[1],
             sourceRect[2], sourceRect[3], targetRect[0], targetRect[1],targetRect[2],targetRect[3]);
	}

	// Validate rectangles:
	if (!ValidatePsychRect(sourceRect) || sourceRect[kPsychLeft]<sourceWin->rect[kPsychLeft] ||
		sourceRect[kPsychTop]<sourceWin->rect[kPsychTop] || sourceRect[kPsychRight]>sourceWin->rect[kPsychRight] ||
		sourceRect[kPsychBottom]>sourceWin->rect[kPsychBottom]) {
		PsychErrorExitMsg(PsychError_user, "Invalid source rectangle specified - (Partially) outside of source window.");
	}
	
	if (!ValidatePsychRect(targetRect) || targetRect[kPsychLeft]<targetWin->rect[kPsychLeft] ||
		targetRect[kPsychTop]<targetWin->rect[kPsychTop] || targetRect[kPsychRight]>targetWin->rect[kPsychRight] ||
		targetRect[kPsychBottom]>targetWin->rect[kPsychBottom]) {
		PsychErrorExitMsg(PsychError_user, "Invalid target rectangle specified - (Partially) outside of target window.");
	}
	
	PsychTestForGLErrors();

	// We have four possible combinations for copy ops:
	// Onscreen -> Onscreen
	// Onscreen -> Texture
	// Texture  -> Texture
	// Texture  -> Onscreen
        
	// Texture -> something copy? (Offscreen to Offscreen or Offscreen to Onscreen)
	// This should work for both, copies from a texture/offscreen window to a different texture/offscreen window/onscreen window,
	// and for copies of a subregion of a texture/offscreen window into a non-overlapping subregion of the texture/offscreen window
	// itself:
	if (sourceWin->windowType == kPsychTexture) {
		// Bind targetWin (texture or onscreen windows framebuffer) as
		// drawing target and just blit texture into it. Binding is done implicitely
		
		if ((sourceWin == targetWin) && (targetWin->imagingMode > 0)) {
			// Copy of a subregion of an offscreen window into itself while imaging pipe active, ie. FBO storage: This is actually the same
			// as on onscreen -> onscreen copy, just with the targetWin FBO bound.
			
			// Set target windows framebuffer as drawing target:
			PsychSetDrawingTarget(targetWin);

			// Disable any shading during copy-op:
			PsychSetShader(targetWin, 0);

			// Start position for pixel write is:
			glRasterPos2f(targetRect[kPsychLeft], targetRect[kPsychBottom]);
			
			// Zoom factor if rectangle sizes don't match:
			glPixelZoom(PsychGetWidthFromRect(targetRect) / PsychGetWidthFromRect(sourceRect), PsychGetHeightFromRect(targetRect) / PsychGetHeightFromRect(sourceRect));
			
			// Perform pixel copy operation:
			glCopyPixels(sourceRect[kPsychLeft], PsychGetHeightFromRect(sourceWin->rect) - sourceRect[kPsychBottom], (int) PsychGetWidthFromRect(sourceRect), (int) PsychGetHeightFromRect(sourceRect), GL_COLOR);
			
			// That's it.
			glPixelZoom(1,1);
			
			// Flush drawing commands and wait for render-completion in single-buffer mode:
			PsychFlushGL(targetWin);				
		}
		else {
				// Sourcewin != Targetwin and/or imaging pipe (FBO storage) not used. We blit the
				// backing texture into itself, aka into its representation inside the system
				// backbuffer. The blit routine will setup proper bindings:

				// We use filterMode == 1 aka Bilinear filtering, so we get nice texture copies
				// if size of sourceRect and targetRect don't match and some scaling is needed.
				// We maybe could map the copyMode argument into some filterMode settings, but
				// i don't know the spec of copyMode, so ...
				PsychBlitTextureToDisplay(sourceWin, targetWin, sourceRect, targetRect, 0, 1, 1);
				
				// That's it.
				
				// Flush drawing commands and wait for render-completion in single-buffer mode:
				PsychFlushGL(targetWin);
		}
	}

	// Onscreen to texture copy?
	if (PsychIsOnscreenWindow(sourceWin) && PsychIsOffscreenWindow(targetWin)) {
		// With the current implemenation we can't zoom if sizes of sourceRect and targetRect don't
		// match: Only one-to-one copy possible...
		if(PsychGetWidthFromRect(sourceRect) != PsychGetWidthFromRect(targetRect) ||
		   PsychGetHeightFromRect(sourceRect) != PsychGetHeightFromRect(targetRect)) {
				// Non-matching sizes. We can't perform requested scaled copy :(
				PsychErrorExitMsg(PsychError_user, "Size mismatch of sourceRect and targetRect. Matching size is required for Onscreen to Offscreen copies. Sorry.");
		}
		
		// Update selected textures content:
		// Looks weird but we need the framebuffer of sourceWin:
		PsychSetDrawingTarget(sourceWin);
		
		// Disable any shading during copy-op:
		PsychSetShader(sourceWin, 0);

		// Texture objects are shared across contexts, so doesn't matter if targetWin's texture actually
		// belongs to the bound context of sourceWin:
		glBindTexture(PsychGetTextureTarget(targetWin), targetWin->textureNumber);
		
		// Copy into texture:
		glCopyTexSubImage2D(PsychGetTextureTarget(targetWin), 0, targetRect[kPsychLeft], PsychGetHeightFromRect(targetWin->rect) - targetRect[kPsychBottom], sourceRect[kPsychLeft], PsychGetHeightFromRect(sourceWin->rect) - sourceRect[kPsychBottom],
							(int) PsychGetWidthFromRect(sourceRect), (int) PsychGetHeightFromRect(sourceRect));
		
		// Unbind texture object:
		glBindTexture(PsychGetTextureTarget(targetWin), 0);
		
		// That's it.
		glPixelZoom(1,1);
	}

	// Onscreen to Onscreen copy?
	if (PsychIsOnscreenWindow(sourceWin) && PsychIsOnscreenWindow(targetWin)) {
		// Currently only works for copies of subregion -> subregion inside same onscreen window,
		// not across different onscreen windows! TODO: Only possible with EXT_framebuffer_blit
		if (sourceWin != targetWin) PsychErrorExitMsg(PsychError_user, "Sorry, the current implementation only supports copies within the same onscreen window, not accross onscreen windows.");

		// Set target windows framebuffer as drawing target:
		PsychSetDrawingTarget(targetWin);
		
		// Disable any shading during copy-op:
		PsychSetShader(targetWin, 0);

		// Start position for pixel write is:
		glRasterPos2f(targetRect[kPsychLeft], targetRect[kPsychBottom]);
		
		// Zoom factor if rectangle sizes don't match:
		glPixelZoom(PsychGetWidthFromRect(targetRect) / PsychGetWidthFromRect(sourceRect), PsychGetHeightFromRect(targetRect) / PsychGetHeightFromRect(sourceRect));
		
		// Perform pixel copy operation:
		glCopyPixels(sourceRect[kPsychLeft], PsychGetHeightFromRect(sourceWin->rect) - sourceRect[kPsychBottom], (int) PsychGetWidthFromRect(sourceRect), (int) PsychGetHeightFromRect(sourceRect), GL_COLOR);
		
		// That's it.
		glPixelZoom(1,1);
		
		// Flush drawing commands and wait for render-completion in single-buffer mode:
		PsychFlushGL(targetWin);
	}

	// Just to make sure we catch invalid values:
	PsychTestForGLErrors();

	// Done.
	return(PsychError_none);
}
