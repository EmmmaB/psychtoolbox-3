/*
	PsychToolbox3/Source/Linux/Screen/PsychScreenGlue.c
	
	PLATFORMS:	
	
		This is the Linux/X11 version only.  
				
	AUTHORS:
	
	Mario Kleiner		mk		mario.kleiner at tuebingen.mpg.de

	HISTORY:
	
	2/20/06                 mk              Wrote it. Derived from Windows version.
        							
	DESCRIPTION:
	
		Functions in this file comprise an abstraction layer for probing and controlling screen state.  
		
		Each C function which implements a particular Screen subcommand should be platform neutral.  For example, the source to SCREENPixelSizes() 
		should be platform-neutral, despite that the calls in OS X and Linux to detect available pixel sizes are
		different.  The platform specificity is abstracted out in C files which end it "Glue", for example PsychScreenGlue, PsychWindowGlue, 
		PsychWindowTextClue.
	
		In addition to glue functions for windows and screen there are functions which implement shared functionality between between Screen commands,
		such as ScreenTypes.c and WindowBank.c. 
			
	NOTES:
	
	TO DO: 

*/


#include "Screen.h"


/* These are needed for our ATI specific beamposition query implementation: */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <asm/page.h>

// We build with VidModeExtension support unless forcefully disabled at compile time via a -DNO_VIDMODEEXTS
#ifndef NO_VIDMODEEXTS
#define USE_VIDMODEEXTS 1
#endif

#ifdef USE_VIDMODEEXTS
// Functions for setup and query of hw gamma CLUTS and for monitor refresh rate query:
#include <X11/extensions/xf86vmode.h>

#else
#define XF86VidModeNumberErrors 0
#endif


// file local variables
/* Following structures are needed by our ATI beamposition query implementation: */
/* Location and format of the relevant hardware registers of the R500/R600 chips
 * was taken from the official register spec for that chips which was released to
 * the public by AMD/ATI end of 2007 and is available for download at XOrg.
 * TODO: Add download link to ATI spec documents!
 *
 * I'm certain this should work on any R500/600 chip, but as i don't have any
 * specs for earlier Radeons, i don't know what happens on such GPU's. If it
 * works, it'll work by accident/luck, but not because we designed for it ;-).
 */
 
/* The method - and small bits of code - for accessing the ATI Radeons registers
 * directly, was taken/borrowed/derived from the useful "Radeontool" utility from
 * Frederick Dean. Below is the copyright notice and credits of Radeontool:
 *
 * Radeontool   v1.4
 * by Frederick Dean <software@fdd.com>
 * Copyright 2002-2004 Frederick Dean
 * Use hereby granted under the zlib license.
 *
 * Warning: I do not have the Radeon documents, so this was engineered from 
 * the radeon_reg.h header file.  
 *
 * USE RADEONTOOL AT YOUR OWN RISK
 *
 * Thanks to Deepak Chawla, Erno Kuusela, Rolf Offermanns, and Soos Peter
 * for patches.
 */


// The D1CRTC_STATUS_POSITION register (32 bits) encodes vertical beam position in
// bits 0:12 (the least significant 13 bits), and horizontal beam position in
// bits 16-28. D2 is the secondary display pipeline (e.g., the external video port
// on a laptop), whereas D1 is the primary pipeline (e.g., internal panel of a laptop).
// The addresses and encoding is the same for the X1000 series and the newer HD2000/3000
// series chips...
#define RADEON_D1CRTC_STATUS_POSITION 0x60a0
#define RADEON_D2CRTC_STATUS_POSITION 0x68a0
#define RADEON_VBEAMPOSITION_BITMASK  0x1fff
#define RADEON_HBEAMPOSITION_BITSHIFT 16

// This (if we would use it) would give access to on-chip frame counters. These increment
// once per video refresh cycle - at the beginning of a new cycle (scanline zero) and
// can be software reset, but normally start at system bootup with zero. Not yet sure
// if we should/would wanna use 'em but we'll see...
#define RADEON_D1CRTC_STATUS_FRAME_COUNT 0x60a4
#define RADEON_D2CRTC_STATUS_FRAME_COUNT 0x68a4


// gfx_cntl_mem is mapped to the actual device's memory mapped control area.
// Not the address but what it points to is volatile.
unsigned char * volatile gfx_cntl_mem = NULL;
unsigned int  gfx_length = 0;

// Count of kernel drivers:
static int    numKernelDrivers = 0;

// Helper routine: Read a single 32 bit unsigned int hardware register at
// offset 'offset' and return its value:
static unsigned int radeon_get(unsigned int offset)
{
    unsigned int value;
    value = *(unsigned int * volatile)(gfx_cntl_mem + offset);  
    return(value);
}

// Helper routine: Write a single 32 bit unsigned int hardware register at
// offset 'offset':
static void radeon_set(unsigned int offset, unsigned int value)
{
    *(unsigned int* volatile)(gfx_cntl_mem + offset) = value;  
}

// Helper routine: mmap() the MMIO memory mapped I/O PCI register space of
// graphics card hardware registers into our address space for easy access
// by the radeon_get() routine. 
static unsigned char * map_device_memory(unsigned int base, unsigned int length) 
{
    int mem_fd;
    unsigned char *device_mem = NULL;
    
    // Open device file /dev/mem -- Raw read/write access to system memory space -- Only root can do this:
    if ((mem_fd = open("/dev/mem", O_RDWR) ) < 0) {
        printf("PTB-WARNING: Beamposition queries unavailable because can't open /dev/mem\nYou must run Matlab/Octave with root privileges for this to work.\n\n");
	return(NULL);
    }
    
    // Try to mmap() the MMIO PCI register space to the block of memory and return a handle/its base address.
    // We only ask for a read-only shared mapping and don't request write-access. This as a child protection
    // as we only need to read registers -- this protects against accidental writes to sensitive device control
    // registers:
    device_mem = (unsigned char *) mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, base);
    
    // Close file handle to /dev/mem. Not needed anymore, our mmap() will keep the mapping until unmapped...
    close(mem_fd);
    
    // Worked?
    if (MAP_FAILED == device_mem) {
	printf("PTB-WARNING: Beamposition queries unavailable because could not mmap() device memory: mmap error [%s]\n", strerror(errno));
    	return(NULL);
    }
    
    // Return memory pointer to mmap()'ed registers...
    gfx_length = length;
    
    return(device_mem);
}

// Helper routine: Unmap gfx card control memory and release associated ressources:
void PsychScreenUnmapDeviceMemory(void)
{
	// Any mapped?
	if (gfx_cntl_mem) {
		// Unmap:
		munmap(gfx_cntl_mem, gfx_length);
		gfx_cntl_mem = NULL;
		gfx_length = 0;
	}
	return;
}

// Helper routine: Check if a supported ATI Radeon card (X1000, HD2000 or HD3000 .... series)
// is installed, detect base address of its register space, mmap() it into our address space.
// This is done by parsing the output of the lspci command...
boolean PsychScreenMapRadeonCntlMemory(void)
{
	int pipefd[2];
	int forkrc;
	FILE *fp;
	char line[1000];
	int base;

	if(pipe(pipefd)) {
		printf("PTB-DEBUG:[In ATI Radeon detection code]: pipe() failure\n");
		return(FALSE);
	}
	forkrc = fork();
	if(forkrc == -1) {
		printf("PTB-DEBUG:[In ATI Radeon detection code] fork() failure\n");
		return(FALSE);
	} else if(forkrc == 0) { /* if child */
		close(pipefd[0]);
		dup2(pipefd[1],1);  /* stdout */
		setenv("PATH","/sbin:/usr/sbin:/bin:/usr/bin",1);
		execlp("lspci","lspci","-v",NULL);
		// This point is normally not reached, unless an error occured:
		printf("PTB-DEBUG:[In ATI Radeon detection code] exec lspci failure\n");
		return(FALSE);
	}
	
	// Parent code: Listen at our end of the pipeline for some news from the 'lspci' command:
	close(pipefd[1]);
	fp = fdopen(pipefd[0],"r");
	if(fp == NULL) {
		printf("PTB-DEBUG:[In ATI Radeon detection code] fdopen error\n");
		return(FALSE);
	}
#if 0
	This is an example output of "lspci -v" ...

	00:1f.6 Modem: Intel Corp. 82801CA/CAM AC 97 Modem (rev 01) (prog-if 00 [Generic])
	Subsystem: PCTel Inc: Unknown device 4c21
	Flags: bus master, medium devsel, latency 0, IRQ 11
	I/O ports at d400 [size=256]
	I/O ports at dc00 [size=128]

	01:00.0 VGA compatible controller: ATI Technologies Inc Radeon Mobility M6 LY (prog-if 00 [VGA])
	Subsystem: Dell Computer Corporation: Unknown device 00e3
	Flags: bus master, VGA palette snoop, stepping, 66Mhz, medium devsel, latency 32, IRQ 11
	Memory at e0000000 (32-bit, prefetchable) [size=128M]
	I/O ports at c000 [size=256]
	Memory at fcff0000 (32-bit, non-prefetchable) [size=64K]
	Expansion ROM at <unassigned> [disabled] [size=128K]
	Capabilities: <available only to root>

	02:00.0 Ethernet controller: 3Com Corporation 3c905C-TX/TX-M [Tornado] (rev 78)
	Subsystem: Dell Computer Corporation: Unknown device 00e3
	Flags: bus master, medium devsel, latency 32, IRQ 11
	I/O ports at ec80 [size=128]
	Memory at f8fffc00 (32-bit, non-prefetchable) [size=128]
	Expansion ROM at f9000000 [disabled] [size=128K]
	Capabilities: <available only to root>

	We need to look through it to find the smaller region base address fcff0000.

#endif

	// Iterate line-by-line over lspci output until the Radeon descriptor block is found:
	while(1) {
		/* for every line up to the "Radeon" string */
		if(fgets(line,sizeof(line),fp) == NULL) {  /* if end of file */
			printf("PTB-INFO: No ATI Radeon hardware found in lspci output. Beamposition queries unsupported.\n");
			close(fp);
			return(FALSE);
		}
		
		if(strstr(line,"Radeon") || strstr(line,"ATI Tech")) {  /* if line contains a "radeon" string */
			break; // Found it!
		}
	};
	
	// Iterate over Radeon descriptor block until the line with the control register mapping is found:
	while(1) { /* for every line up till memory statement */
		if(fgets(line,sizeof(line),fp) == NULL || line[0] != '\t') {  /* if end of file */
			printf("PTB-INFO: ATI Radeon control memory not found in lspci output. Beamposition queries unsupported.\n");
			close(fp);
			return(FALSE);
		}
		
		if(0) printf("%s",line);
		
		if(strstr(line,"emory") && strstr(line,"K")) {  /* if line contains a "Memory" and "K" string */
				break; // Found it! This line contains the base address...
		}
	};
	
	// Close lspci output and comm-pipe:
	close(fp);
	
	// Extract base address from config-line:
	if(sscanf(line,"%*s%*s%x",&base) == 0) { /* third token as hex number */
		printf("PTB-INFO: ATI Radeon control memory not found in lspci output [parse error of lspci output]. Beamposition queries unsupported.\n");
		return(FALSE);
	}
	
	// Got it!
	printf("PTB-INFO: ATI-Radeon found. Base control address is %x.\n",base);
	
	// mmap() the PCI register space into our memory: Currently we map 0x8000 bytes, although the actual
	// configuration space would be 0xffff bytes, but we neither need, nor know what the upper regions of
	// this space do, so no need to map'em: gfx_cntl_mem will contain the base of the register block,
	// all register addresses in the official Radeon specs are offsets to that base address. This will
	// return NULL if the mapping fails, e.g., due to insufficient permissions etc...
	gfx_cntl_mem = map_device_memory(base, 0x8000);
	
	// Return final success or failure status:
	return((gfx_cntl_mem) ? TRUE : FALSE);
}


// Maybe use NULLs in the settings arrays to mark entries invalid instead of using boolean flags in a different array.   
static boolean			displayLockSettingsFlags[kPsychMaxPossibleDisplays];
static CFDictionaryRef	        displayOriginalCGSettings[kPsychMaxPossibleDisplays];        	//these track the original video state before the Psychtoolbox changed it.  
static boolean			displayOriginalCGSettingsValid[kPsychMaxPossibleDisplays];
static CFDictionaryRef	        displayOverlayedCGSettings[kPsychMaxPossibleDisplays];        	//these track settings overlayed with 'Resolutions'.  
static boolean			displayOverlayedCGSettingsValid[kPsychMaxPossibleDisplays];
static CGDisplayCount 		numDisplays;

// displayCGIDs stores the X11 Display* handles to the display connections of each PTB logical screen:
static CGDirectDisplayID 	displayCGIDs[kPsychMaxPossibleDisplays];
// displayX11Screens stores the mapping of PTB screenNumber's to corresponding X11 screen numbers:
static int                      displayX11Screens[kPsychMaxPossibleDisplays];
// Maps screenid's to Graphics hardware pipelines: Used to choose pipeline for beampos-queries...
static unsigned char		displayScreensToPipes[kPsychMaxPossibleDisplays];

// X11 has a different - and much more powerful and flexible - concept of displays than OS-X or Windows:
// One can have multiple X11 connections to different logical displays. A logical display corresponds
// to a specific X-Server. This X-Server could run on the same machine as Matlab+PTB or on a different
// machine connected via network somewhere in the building or the world. A single machine can even run
// multiple X-Servers. Each display itself can consist of multiple screens. Each screen represents
// a single physical display device. E.g., a dual-head gfx-adaptor could be driven by a single X-Server and have
// two screens for each physical output. A single X-Server could also drive multiple different gfx-cards
// and therefore have many screens. A Linux render-cluster could consist of multiple independent machines,
// each with multiple screens aka gfx heads connected to each machine (aka X11 display).
//
// By default, PTB just connects to the same display as the one that Matlab is running on and tries to
// detect and enumerate all physical screens connected to that display. The default display is set either
// via Matlab command option "-display" or via the Shell environment variable $DISPLAY. Typically, it
// is simply $DISPLAY=:0.0, which means the local gfx-adaptor attached to the machine the user is logged into.
//
// If a user wants to make use of other displays than the one Matlab is running on, (s)he can set the
// environment variable $PSYCHTOOLBOX_DISPLAYS to a list of all requested displays. PTB will then try
// to connect to each of the listed displays, enumerate all attached screens and build its list of
// available screens as a merge of all screens of all displays.
// E.g., export PSYCHTOOLBOX_DISPLAYS=":0.0,kiwi.kyb.local:0.0,coriander.kyb.local:0.0" would enumerate
// all screens of all gfx-adaptors on the local machine ":0.0", and the network connected machines
// "kiwi.kyb.local" and "coriander.kyb.local".
//
// Possible applications: Multi-display setups on Linux, possibly across machines, e.g., render-clusters
// Weird experiments with special setups. Show stimulus on display 1, query mouse or keyboard from
// different machine... 

static int x11_errorval = 0;
static int x11_errorbase = 0;
static int (*x11_olderrorhandler)(Display*, XErrorEvent*);

//file local functions
void InitCGDisplayIDList(void);
void PsychLockScreenSettings(int screenNumber);
void PsychUnlockScreenSettings(int screenNumber);
boolean PsychCheckScreenSettingsLock(int screenNumber);
//boolean PsychGetCGModeFromVideoSetting(CFDictionaryRef *cgMode, PsychScreenSettingsType *setting);
void InitPsychtoolboxKernelDriverInterface(void);

// Error callback handler for X11 errors:
static int x11VidModeErrorHandler(Display* dis, XErrorEvent* err)
{
  // If x11_errorbase not yet setup, simply return and ignore this error:
  if (x11_errorbase == 0) return(0);

  // Setup: Check if its an XVidMode-Error - the only one we do handle.
  if (err->error_code >=x11_errorbase && err->error_code < x11_errorbase + XF86VidModeNumberErrors ||
      err->error_code == BadValue) {
    // We caused some error. Set error flag:
    x11_errorval = 1;
  }

  // Done.
  return(0);
}

//Initialization functions
void InitializePsychDisplayGlue(void)
{
    int i;
    
    //init the display settings flags.
    for(i=0;i<kPsychMaxPossibleDisplays;i++){
        displayLockSettingsFlags[i]=FALSE;
        displayOriginalCGSettingsValid[i]=FALSE;
        displayOverlayedCGSettingsValid[i]=FALSE;
	displayScreensToPipes[i]=i;
    }
    
    //init the list of Core Graphics display IDs.
    InitCGDisplayIDList();

    // Attach to kernel-level Psychtoolbox graphics card interface driver if possible
    // *and* allowed by settings, setup all relevant mappings:
    InitPsychtoolboxKernelDriverInterface();
}

void InitCGDisplayIDList(void)
{  
  int i, j, k, count, scrnid;
  char* ptbdisplays = NULL;
  char* ptbpipelines = NULL;
  char displayname[1000];
  CGDirectDisplayID x11_dpy = NULL;
 
  // NULL-out array of displays:
  for(i=0;i<kPsychMaxPossibleDisplays;i++) displayCGIDs[i]=NULL;

  // Initial count of screens is zero:
  numDisplays = 0;

  // Multiple X11 display specifier strings provided in the environment variable
  // $PSYCHTOOLBOX_DISPLAYS? If so, we connect to all of them and enumerate all
  // available screens on them.
  ptbdisplays = getenv("PSYCHTOOLBOX_DISPLAYS");
  if (ptbdisplays) {
    // Displays explicitely specified. Parse the string and connect to all of them:
    j=0;
    for (i=0; i<=strlen(ptbdisplays) && j<1000; i++) {
      // Accepted separators are ',', '"', white-space and end of string...
      if (ptbdisplays[i]==',' || ptbdisplays[i]=='"' || ptbdisplays[i]==' ' || i==strlen(ptbdisplays)) {
	// Separator or end of string detected. Try to connect to display:
	displayname[j]=0;
	printf("PTB-INFO: Trying to connect to X-Display %s ...", displayname);

	x11_dpy = XOpenDisplay(displayname);
	if (x11_dpy == NULL) {
	  // Failed.
	  printf(" ...Failed! Skipping this display...\n");
	}
	else {
	  // Query number of available screens on this X11 display:
	  count=ScreenCount(x11_dpy);
	  scrnid=0;

	  // Set the screenNumber --> X11 display mappings up:
	  for (k=numDisplays; (k<numDisplays + count) && (k<kPsychMaxPossibleDisplays); k++) {
	    // Mapping of logical screenNumber to X11 Display:
	    displayCGIDs[k]= x11_dpy;
	    // Mapping of logical screenNumber to X11 screenNumber for X11 Display:
	    displayX11Screens[k]=scrnid++;
	  }

	  printf(" ...success! Added %i new physical display screens of %s as PTB screens %i to %i.\n",
		 scrnid, displayname, numDisplays, k-1);

	  // Update total count:
	  numDisplays = k;
	}

	// Reset idx:
	j=0;
      }
      else {
	// Add character to display name:
	displayname[j++]=ptbdisplays[i];
      }
    }
    
    // At least one screen enumerated?
    if (numDisplays < 1) {
      // We're screwed :(
      PsychErrorExitMsg(PsychError_internal, "FATAL ERROR: Couldn't open any X11 display connection to any X-Server!!!");
    }
  }
  else {
    // User didn't setup env-variable with any special displays. We just use
    // the default $DISPLAY or -display of Matlab:
    x11_dpy = XOpenDisplay(NULL);
    if (x11_dpy == NULL) {
      // We're screwed :(
      PsychErrorExitMsg(PsychError_internal, "FATAL ERROR: Couldn't open default X11 display connection to X-Server!!!");
    }
    
    // Query number of available screens on this X11 display:
    count=ScreenCount(x11_dpy);

    // Set the screenNumber --> X11 display mappings up:
    for (i=0; i<count && i<kPsychMaxPossibleDisplays; i++) { displayCGIDs[i]= x11_dpy; displayX11Screens[i]=i; }
    numDisplays=i;
  }

  if (numDisplays>1) printf("PTB-Info: A total of %i physical X-Windows display screens is available for use.\n", numDisplays);
  fflush(NULL);

  // Did user provide an override for the screenid --> pipeline mapping?
  ptbpipelines = getenv("PSYCHTOOLBOX_PIPEMAPPINGS");
  if (ptbpipelines) {
	// Yep. Format is: One character (a number between "0" and "9") for each screenid,
	// e.g., "021" would map screenid 0 to pipe 0, screenid 1 to pipe 2 and screenid 2 to pipe 1.
	// The default is "012...", ie screen 0 = pipe 0, 1 = pipe 1, 2 =pipe 2, n = pipe n
	for (j=0; j<strlen(ptbpipelines) && j<numDisplays; j++) {
	   displayScreensToPipes[j] = ((ptbpipelines[j] - 0x30)>=0 && (ptbpipelines[j] - 0x30)<kPsychMaxPossibleDisplays) ? (ptbpipelines[j] - 0x30) : 0;
	}
  }

  return;
}

int PsychGetXScreenIdForScreen(int screenNumber)
{
  if(screenNumber>=numDisplays) PsychErrorExit(PsychError_invalidScumber);
  return(displayX11Screens[screenNumber]);
}

void PsychGetCGDisplayIDFromScreenNumber(CGDirectDisplayID *displayID, int screenNumber)
{
    if(screenNumber>=numDisplays) PsychErrorExit(PsychError_invalidScumber);
    *displayID=displayCGIDs[screenNumber];
}


/*  About locking display settings:

    SCREENOpenWindow and SCREENOpenOffscreenWindow  set the lock when opening  windows and 
    SCREENCloseWindow unsets upon the close of the last of a screen's windows. PsychSetVideoSettings checks for a lock
    before changing the settings.  Anything (SCREENOpenWindow or SCREENResolutions) which attemps to change
    the display settings should report that attempts to change a dipslay's settings are not allowed when its windows are open.
    
    PsychSetVideoSettings() may be called by either SCREENOpenWindow or by Resolutions().  If called by Resolutions it both sets the video settings
    and caches the video settings so that subsequent calls to OpenWindow can use the cached mode regardless of whether interceding calls to OpenWindow
    have changed the display settings or reverted to the virgin display settings by closing.  SCREENOpenWindow should thus invoke the cached mode
    settings if they have been specified and not current actual display settings.  
    
*/    
void PsychLockScreenSettings(int screenNumber)
{
    displayLockSettingsFlags[screenNumber]=TRUE;
}

void PsychUnlockScreenSettings(int screenNumber)
{
    displayLockSettingsFlags[screenNumber]=FALSE;
}

boolean PsychCheckScreenSettingsLock(int screenNumber)
{
    return(displayLockSettingsFlags[screenNumber]);
}


/* Because capture and lock will always be used in conjuction, capture calls lock, and SCREENOpenWindow must only call Capture and Release */
void PsychCaptureScreen(int screenNumber)
{
    CGDisplayErr  error=0;
    
    if(screenNumber>=numDisplays) PsychErrorExit(PsychError_invalidScumber);

    // MK: We could do this to get exclusive access to the X-Server, but i'm too
    // scared of doing it at the moment:
    // XGrabServer(displayCGIDs[screenNumber]);

    if(error) PsychErrorExitMsg(PsychError_internal, "Unable to capture display");
    PsychLockScreenSettings(screenNumber);
}

/*
    PsychReleaseScreen()    
*/
void PsychReleaseScreen(int screenNumber)
{	
    CGDisplayErr  error=0;
    
    if(screenNumber>=numDisplays) PsychErrorExit(PsychError_invalidScumber);
    // MK: We could do this to release exclusive access to the X-Server, but i'm too
    // scared of doing it at the moment:
    // XUngrabServer(displayCGIDs[screenNumber]);

    // On Linux we restore the original display settings of the to be released screen:
    PsychRestoreScreenSettings(screenNumber);
    if(error) PsychErrorExitMsg(PsychError_internal, "Unable to release display");
    PsychUnlockScreenSettings(screenNumber);
}

boolean PsychIsScreenCaptured(screenNumber)
{
    return(PsychCheckScreenSettingsLock(screenNumber));
}    


//Read display parameters.
/*
    PsychGetNumDisplays()
    Get the number of video displays connected to the system.
*/

int PsychGetNumDisplays(void)
{
    return((int) numDisplays);
}

void PsychGetScreenDepths(int screenNumber, PsychDepthType *depths)
{
  int* x11_depths;
  int  i, count;

  if(screenNumber>=numDisplays) PsychErrorExitMsg(PsychError_internal, "screenNumber is out of range"); //also checked within SCREENPixelSizes

  x11_depths = XListDepths(displayCGIDs[screenNumber], PsychGetXScreenIdForScreen(screenNumber), &count);
  if (depths && count>0) {
    // Query successful: Add all values to depth struct:
    for(i=0; i<count; i++) PsychAddValueToDepthStruct(x11_depths[i], depths);
    XFree(x11_depths);
  }
  else {
    // Query failed: Assume at least 32 bits is available.
    printf("PTB-WARNING: Couldn't query available display depths values! Returning a made up list...\n");
    fflush(NULL);
    PsychAddValueToDepthStruct(32, depths);
    PsychAddValueToDepthStruct(24, depths);
    PsychAddValueToDepthStruct(16, depths); 
  }
}

/*   PsychGetAllSupportedScreenSettings()
 *
 *	 Queries the display system for a list of all supported display modes, ie. all valid combinations
 *	 of resolution, pixeldepth and refresh rate. Allocates temporary arrays for storage of this list
 *	 and returns it to the calling routine. This function is basically only used by Screen('Resolutions').
 */
int PsychGetAllSupportedScreenSettings(int screenNumber, long** widths, long** heights, long** hz, long** bpp)
{
//    int i, rc, numPossibleModes;
//    DEVMODE result;
//    long tempWidth, tempHeight, currentFrequency, tempFrequency, tempDepth;
//
//    if(screenNumber>=numDisplays) PsychErrorExit(PsychError_invalidScumber);
//
//	// First pass: How many modes are supported?
//	i=-1;
//	do {
//        result.dmSize = sizeof(DEVMODE);
//        result.dmDriverExtra = 0;
//        rc = EnumDisplaySettings(PsychGetDisplayDeviceName(screenNumber), i, &result);
//        i++;
//	} while (rc!=0);
//
//    // Get a list of avialable modes for the specified display:
//    numPossibleModes= i;
//	
//	// Allocate output arrays: These will get auto-released at exit
//	// from Screen():
//	*widths = (long*) PsychMallocTemp(numPossibleModes * sizeof(int));
//	*heights = (long*) PsychMallocTemp(numPossibleModes * sizeof(int));
//	*hz = (long*) PsychMallocTemp(numPossibleModes * sizeof(int));
//	*bpp = (long*) PsychMallocTemp(numPossibleModes * sizeof(int));
//	
//	// Fetch modes and store into arrays:
//    for(i=0; i < numPossibleModes; i++) {
//        result.dmSize = sizeof(DEVMODE);
//        result.dmDriverExtra = 0;
//        rc = EnumDisplaySettings(PsychGetDisplayDeviceName(screenNumber), i, &result);
//		(*widths)[i] = (long)  result.dmPelsWidth;
//		(*heights)[i] = (long)  result.dmPelsHeight;
//		(*hz)[i] = (long) result.dmDisplayFrequency;
//		(*bpp)[i] = (long) result.dmBitsPerPel;
//    }
//
//	return(numPossibleModes);

	// FIXME: Not yet implemented!
	return(0);
}

/*
    static PsychGetCGModeFromVideoSettings()
   
*/
boolean PsychGetCGModeFromVideoSetting(CFDictionaryRef *cgMode, PsychScreenSettingsType *setting)
{
  // Dummy assignment:
  *cgMode = 1;
  return(TRUE);
}


/*
    PsychCheckVideoSettings()
    
    Check all available video display modes for the specified screen number and return true if the 
    settings are valid and false otherwise.
*/
boolean PsychCheckVideoSettings(PsychScreenSettingsType *setting)
{
        CFDictionaryRef cgMode;       
        return(PsychGetCGModeFromVideoSetting(&cgMode, setting));
}

/*
    PsychGetScreenDepth()
    
    The caller must allocate and initialize the depth struct. 
*/
void PsychGetScreenDepth(int screenNumber, PsychDepthType *depth)
{    
  if(screenNumber>=numDisplays) PsychErrorExitMsg(PsychError_internal, "screenNumber is out of range"); //also checked within SCREENPixelSizes
  PsychAddValueToDepthStruct(DefaultDepth(displayCGIDs[screenNumber], PsychGetXScreenIdForScreen(screenNumber)), depth);
}

int PsychGetScreenDepthValue(int screenNumber)
{
    PsychDepthType	depthStruct;
    
    PsychInitDepthStruct(&depthStruct);
    PsychGetScreenDepth(screenNumber, &depthStruct);
    return(PsychGetValueFromDepthStruct(0,&depthStruct));
}


float PsychGetNominalFramerate(int screenNumber)
{
#ifdef USE_VIDMODEEXTS

  // Information returned by the XF86VidModeExtension:
  XF86VidModeModeLine mode_line;  // The mode line of the current video mode.
  int dot_clock;                  // The RAMDAC / TDMS pixel clock frequency.

  // We start with a default vrefresh of zero, which means "couldn't query refresh from OS":
  float vrefresh = 0;

  if(screenNumber>=numDisplays)
    PsychErrorExitMsg(PsychError_internal, "screenNumber passed to PsychGetScreenDepths() is out of range"); 

  if (!XF86VidModeSetClientVersion(displayCGIDs[screenNumber])) {
    // Failed to use VidMode-Extension. We just return a vrefresh of zero.
    return(0);
  }

  // Query vertical refresh rate. If it fails we default to the last known good value...
  if (XF86VidModeGetModeLine(displayCGIDs[screenNumber], PsychGetXScreenIdForScreen(screenNumber), &dot_clock, &mode_line)) {
    // Vertical refresh rate is: RAMDAC pixel clock / width of a scanline in clockcylces /
    // number of scanlines per videoframe.
    vrefresh = (((dot_clock * 1000) / mode_line.htotal) * 1000) / mode_line.vtotal;

    // Divide vrefresh by 1000 to get real Hz - value:
    vrefresh = vrefresh / 1000.0f;
  }

  // Done.
  return(vrefresh);
#else
  return(0);
#endif
}

float PsychSetNominalFramerate(int screenNumber, float requestedHz)
{
#ifdef USE_VIDMODEEXTS

  // Information returned by/sent to the XF86VidModeExtension:
  XF86VidModeModeLine mode_line;  // The mode line of the current video mode.
  int dot_clock;                  // The RAMDAC / TDMS pixel clock frequency.
  int rc;
  int event_base;

  // We start with a default vrefresh of zero, which means "couldn't query refresh from OS":
  float vrefresh = 0;

  if(screenNumber>=numDisplays)
    PsychErrorExitMsg(PsychError_internal, "screenNumber is out of range"); 

  if (!XF86VidModeSetClientVersion(displayCGIDs[screenNumber])) {
    // Failed to use VidMode-Extension. We just return a vrefresh of zero.
    return(0);
  }

  if (!XF86VidModeQueryExtension(displayCGIDs[screenNumber], &event_base, &x11_errorbase)) {
    // Failed to use VidMode-Extension. We just return a vrefresh of zero.
    return(0);
  }

  // Attach our error callback handler and reset error-state:
  x11_errorval = 0;
  x11_olderrorhandler = XSetErrorHandler(x11VidModeErrorHandler);

  // Step 1: Query current dotclock and modeline:
  if (!XF86VidModeGetModeLine(displayCGIDs[screenNumber], PsychGetXScreenIdForScreen(screenNumber), &dot_clock, &mode_line)) {
    // Restore default error handler:
    XSetErrorHandler(x11_olderrorhandler);

    PsychErrorExitMsg(PsychError_internal, "Failed to query video dotclock and modeline!"); 
  }

  // Step 2: Calculate updated modeline:
  if (requestedHz > 10) {
    // Step 2-a: Given current dot-clock and modeline and requested vrefresh, compute
    // modeline for closest possible match:
    requestedHz*=1000.0f;
    vrefresh = (((dot_clock * 1000) / mode_line.htotal) * 1000) / requestedHz;
    
    // Assign it to closest modeline setting:
    mode_line.vtotal = (int)(vrefresh + 0.5f);
  }
  else {
    // Step 2-b: Delta mode. requestedHz represents a direct integral offset
    // to add or subtract from current modeline setting:
    mode_line.vtotal+=(int) requestedHz;
  }

  // Step 3: Try to set new modeline:
  if (!XF86VidModeModModeLine(displayCGIDs[screenNumber], PsychGetXScreenIdForScreen(screenNumber), &mode_line)) {
    // Restore default error handler:
    XSetErrorHandler(x11_olderrorhandler);

    // Invalid modeline? Signal this:
    return(-1);
  }

  // We synchronize and wait for X-Request completion. If the modeline was invalid,
  // this will trigger an invocation of our errorhandler, which in turn will
  // set the x11_errorval to a non-zero value:
  XSync(displayCGIDs[screenNumber], FALSE);
  
  // Restore default error handler:
  XSetErrorHandler(x11_olderrorhandler);

  // Check for error:
  if (x11_errorval) {
    // Failed to set new mode! Must be invalid. We return -1 to signal this:
    return(-1);
  }

  // No error...

  // Step 4: Query new settings and return them:
  vrefresh = PsychGetNominalFramerate(screenNumber);

  // Done.
  return(vrefresh);
#else
  return(0);
#endif
}

/* Returns the physical display size as reported by X11: */
void PsychGetDisplaySize(int screenNumber, int *width, int *height)
{
    if(screenNumber>=numDisplays)
        PsychErrorExitMsg(PsychError_internal, "screenNumber passed to PsychGetDisplaySize() is out of range");
    *width = (int) XDisplayWidthMM(displayCGIDs[screenNumber], PsychGetXScreenIdForScreen(screenNumber));
    *height = (int) XDisplayHeightMM(displayCGIDs[screenNumber], PsychGetXScreenIdForScreen(screenNumber));
}

void PsychGetScreenSize(int screenNumber, long *width, long *height)
{
  if(screenNumber>=numDisplays) PsychErrorExitMsg(PsychError_internal, "screenNumber passed to PsychGetScreenDepths() is out of range"); 
  *width=XDisplayWidth(displayCGIDs[screenNumber], PsychGetXScreenIdForScreen(screenNumber));
  *height=XDisplayHeight(displayCGIDs[screenNumber], PsychGetXScreenIdForScreen(screenNumber));
}


void PsychGetGlobalScreenRect(int screenNumber, double *rect)
{
  // Create an empty rect:
  PsychMakeRect(rect, 0, 0, 1, 1);
  // Fill it with meaning by PsychGetScreenRect():
  PsychGetScreenRect(screenNumber, rect);
}


void PsychGetScreenRect(int screenNumber, double *rect)
{
    long width, height; 

    PsychGetScreenSize(screenNumber, &width, &height);
    rect[kPsychLeft]=0;
    rect[kPsychTop]=0;
    rect[kPsychRight]=(int)width;
    rect[kPsychBottom]=(int)height; 
} 


PsychColorModeType PsychGetScreenMode(int screenNumber)
{
    PsychDepthType depth;
        
    PsychInitDepthStruct(&depth);
    PsychGetScreenDepth(screenNumber, &depth);
    return(PsychGetColorModeFromDepthStruct(&depth));
}


/*
    Its probably better to read this directly from the CG renderer info than to infer it from the pixel size
*/	
int PsychGetNumScreenPlanes(int screenNumber)
{    
    return((PsychGetScreenDepthValue(screenNumber)>24) ? 4 : 3);
}



/*
	This is a place holder for a function which uncovers the number of dacbits.  To be filled in at a later date.
	If you know that your card supports >8 then you can fill that in the PsychtPreferences and the psychtoolbox
	will act accordingly.
	
	There seems to be no way to uncover the dacbits programatically.  According to apple CoreGraphics
	sends a 16-bit word and the driver throws out whatever it chooses not to use.
		
	For now we just use 8 to avoid false precision.  
	
	If we can uncover the video card model then  we can implement a table lookup of video card model to number of dacbits.  
*/
int PsychGetDacBitsFromDisplay(int screenNumber)
{
	return(8);
}



/*
    PsychGetVideoSettings()
    
    Fills a structure describing the screen settings such as x, y, depth, frequency, etc.
    
    Consider inverting the calling sequence so that this function is at the bottom of call hierarchy.  
*/ 
void PsychGetScreenSettings(int screenNumber, PsychScreenSettingsType *settings)
{
    settings->screenNumber=screenNumber;
    PsychGetScreenRect(screenNumber, settings->rect);
    PsychInitDepthStruct(&(settings->depth));
    PsychGetScreenDepth(screenNumber, &(settings->depth));
    settings->mode=PsychGetColorModeFromDepthStruct(&(settings->depth));
    settings->nominalFrameRate=PsychGetNominalFramerate(screenNumber);
    //settings->dacbits=PsychGetDacBits(screenNumber);
}




//Set display parameters

/*
    PsychSetScreenSettings()
	
    Accept a PsychScreenSettingsType structure holding a video mode and set the display mode accordingly.
    
    If we can not change the display settings because of a lock (set by open window or close window) then return false.
    
    SCREENOpenWindow should capture the display before it sets the video mode.  If it doesn't, then PsychSetVideoSettings will
    detect that and exit with an error.  SCREENClose should uncapture the display. 
    
    The duties of SCREENOpenWindow are:
    -Lock the screen which serves the purpose of preventing changes in video setting with open Windows.
    -Capture the display which gives the application synchronous control of display parameters.
    
    TO DO: for 8-bit palletized mode there is probably more work to do.  
      
*/

boolean PsychSetScreenSettings(boolean cacheSettings, PsychScreenSettingsType *settings)
{
    CFDictionaryRef 		cgMode;
    boolean 			isValid, isCaptured;
    CGDisplayErr 		error;

    //get the display IDs.  Maybe we should consolidate this out of these functions and cache the IDs in a file static
    //variable, since basicially every core graphics function goes through this deal.    
    if(settings->screenNumber>=numDisplays)
        PsychErrorExitMsg(PsychError_internal, "screenNumber passed to PsychSetScreenSettings() is out of range");

    //Check for a lock which means onscreen or offscreen windows tied to this screen are currently open.
    // MK Disabled: if(PsychCheckScreenSettingsLock(settings->screenNumber)) return(false);  //calling function should issue an error for attempt to change display settings while windows were open.
    
    //store the original display mode if this is the first time we have called this function.  The psychtoolbox will disregard changes in 
    //the screen state made through the control panel after the Psychtoolbox was launched. That is, OpenWindow will by default continue to 
    //open windows with finder settings which were in place at the first call of OpenWindow.  That's not intuitive, but not much of a problem
    //either. 
    if(!displayOriginalCGSettingsValid[settings->screenNumber]){
      displayOriginalCGSettings[settings->screenNumber]= 1; // FIXME!!! CGDisplayCurrentMode(displayCGIDs[settings->screenNumber]);
      displayOriginalCGSettingsValid[settings->screenNumber]=TRUE;
    }
    
    //Find core graphics video settings which correspond to settings as specified withing by an abstracted psychsettings structure.  
    isValid=PsychGetCGModeFromVideoSetting(&cgMode, settings);
    if(!isValid){
        PsychErrorExitMsg(PsychError_internal, "Attempt to set invalid video settings"); 
        //this is an internal error because the caller is expected to check first. 
    }
    
    //If the caller passed cache settings (then it is SCREENResolutions) and we should cache the current video mode settings for this display.  These
    //are cached in the form of CoreGraphics settings and not Psychtoolbox video settings.  The only caller which should pass a set cache flag is 
    //SCREENResolutions
    if(cacheSettings){
        displayOverlayedCGSettings[settings->screenNumber]=cgMode;
        displayOverlayedCGSettingsValid[settings->screenNumber]=TRUE;
    }
    
    //Check to make sure that this display is captured, which OpenWindow should have done.  If it has not been done, then exit with an error.  
    isCaptured=PsychIsScreenCaptured(settings->screenNumber);
    if(!isCaptured) PsychErrorExitMsg(PsychError_internal, "Attempt to change video settings without capturing the display");
        
    //Change the display mode.   
    // FIXME: Not yet implemented.
    
    return(true);
}

/*
    PsychRestoreVideoSettings()
    
    Restores video settings to the state set by the finder.  Returns TRUE if the settings can be restored or false if they 
    can not be restored because a lock is in effect, which would mean that there are still open windows.    
    
*/
boolean PsychRestoreScreenSettings(int screenNumber)
{
    boolean 			isCaptured;
    CGDisplayErr 		error=0;


    if(screenNumber>=numDisplays)
        PsychErrorExitMsg(PsychError_internal, "screenNumber passed to PsychGetScreenDepths() is out of range"); //also checked within SCREENPixelSizes

    //Check for a lock which means onscreen or offscreen windows tied to this screen are currently open.
    // if(PsychCheckScreenSettingsLock(screenNumber)) return(false);  //calling function will issue error for attempt to change display settings while windows were open.
    
    //Check to make sure that the original graphics settings were cached.  If not, it means that the settings were never changed, so we can just
    //return true. 
    if(!displayOriginalCGSettingsValid[screenNumber])
        return(true);
    
    //Check to make sure that this display is captured, which OpenWindow should have done.  If it has not been done, then exit with an error.  
    isCaptured=PsychIsScreenCaptured(screenNumber);
    if(!isCaptured) PsychErrorExitMsg(PsychError_internal, "Attempt to change video settings without capturing the display");
    
    // FIXME: Not yet implemented...
    return(true);
}


void PsychHideCursor(int screenNumber)
{
  // Static "Cursor" object which defines a completely transparent - and therefore invisible
  // X11 cursor for the mouse-pointer.
  static Cursor nullCursor = -1;

  // Check for valid screenNumber:
  if(screenNumber>=numDisplays) PsychErrorExitMsg(PsychError_internal, "screenNumber passed to PsychHideCursor() is out of range"); //also checked within SCREENPixelSizes

  // nullCursor already ready?
  if( nullCursor == (Cursor) -1 ) {
    // Create one:
    Pixmap cursormask;
    XGCValues xgc;
    GC gc;
    XColor dummycolour;

    cursormask = XCreatePixmap(displayCGIDs[screenNumber], RootWindow(displayCGIDs[screenNumber], PsychGetXScreenIdForScreen(screenNumber)), 1, 1, 1/*depth*/);
    xgc.function = GXclear;
    gc = XCreateGC(displayCGIDs[screenNumber], cursormask, GCFunction, &xgc );
    XFillRectangle(displayCGIDs[screenNumber], cursormask, gc, 0, 0, 1, 1 );
    dummycolour.pixel = 0;
    dummycolour.red   = 0;
    dummycolour.flags = 04;
    nullCursor = XCreatePixmapCursor(displayCGIDs[screenNumber], cursormask, cursormask, &dummycolour, &dummycolour, 0, 0 );
    XFreePixmap(displayCGIDs[screenNumber], cursormask );
    XFreeGC(displayCGIDs[screenNumber], gc );
  }

  // Attach nullCursor to our onscreen window:
  XDefineCursor(displayCGIDs[screenNumber], RootWindow(displayCGIDs[screenNumber], PsychGetXScreenIdForScreen(screenNumber)), nullCursor );
  XFlush(displayCGIDs[screenNumber]);

  return;
}

void PsychShowCursor(int screenNumber)
{
  // Check for valid screenNumber:
  if(screenNumber>=numDisplays) PsychErrorExitMsg(PsychError_internal, "screenNumber passed to PsychHideCursor() is out of range"); //also checked within SCREENPixelSizes
  // Reset to default system cursor, which is a visible one.
  XUndefineCursor(displayCGIDs[screenNumber], RootWindow(displayCGIDs[screenNumber], PsychGetXScreenIdForScreen(screenNumber)));
  XFlush(displayCGIDs[screenNumber]);
}

void PsychPositionCursor(int screenNumber, int x, int y)
{
  // Reposition the mouse cursor:
  if (XWarpPointer(displayCGIDs[screenNumber], None, RootWindow(displayCGIDs[screenNumber], PsychGetXScreenIdForScreen(screenNumber)), 0, 0, 0, 0, x, y)==BadWindow) {
    PsychErrorExitMsg(PsychError_internal, "Couldn't position the mouse cursor! (XWarpPointer() failed).");
  }
  XFlush(displayCGIDs[screenNumber]);
}

/*
    PsychReadNormalizedGammaTable()
    
    TO DO: This should probably be changed so that the caller allocates the memory.
    TO DO: Adopt a naming convention which distinguishes between functions which allocate memory for return variables from those which do not.
            For example, PsychReadNormalizedGammaTable() vs. PsychGetNormalizedGammaTable().
    
*/
void PsychReadNormalizedGammaTable(int screenNumber, int *numEntries, float **redTable, float **greenTable, float **blueTable)
{
#ifdef USE_VIDMODEEXTS

  CGDirectDisplayID	cgDisplayID;
  static  float localRed[256], localGreen[256], localBlue[256];
  
  // The X-Windows hardware LUT has 3 tables for R,G,B, 256 slots each.
  // Each entry is a 16-bit word with the n most significant bits used for an n-bit DAC.
  psych_uint16	RTable[256];
  psych_uint16	GTable[256];
  psych_uint16	BTable[256];
  int     i;        

  // Query OS for gamma table:
  PsychGetCGDisplayIDFromScreenNumber(&cgDisplayID, screenNumber);
  XF86VidModeGetGammaRamp(cgDisplayID, PsychGetXScreenIdForScreen(screenNumber), 256, (unsigned short*) RTable, (unsigned short*) GTable, (unsigned short*) BTable);

  // Convert tables:Map 16-bit values into 0-1 normalized floats:
  *redTable=localRed; *greenTable=localGreen; *blueTable=localBlue;
  for (i=0; i<256; i++) localRed[i]   = ((float) RTable[i]) / 65535.0f;
  for (i=0; i<256; i++) localGreen[i] = ((float) GTable[i]) / 65535.0f;
  for (i=0; i<256; i++) localBlue[i]  = ((float) BTable[i]) / 65535.0f;

  // The LUT's always have 256 slots for the 8-bit framebuffer:
  *numEntries= 256;
#endif
  return;
}

void PsychLoadNormalizedGammaTable(int screenNumber, int numEntries, float *redTable, float *greenTable, float *blueTable)
{
#ifdef USE_VIDMODEEXTS

  CGDirectDisplayID	cgDisplayID;
  int     i;        
  psych_uint16	RTable[256];
  psych_uint16	GTable[256];
  psych_uint16	BTable[256];

  // Table must have 256 slots!
  if (numEntries!=256) PsychErrorExitMsg(PsychError_user, "Loadable hardware gamma tables must have 256 slots!");    
  
  // The X-Windows hardware LUT has 3 tables for R,G,B, 256 slots each.
  // Each entry is a 16-bit word with the n most significant bits used for an n-bit DAC.

  // Convert input table to X11 specific gammaTable:
  for (i=0; i<256; i++) RTable[i] = (int)(redTable[i]   * 65535.0f + 0.5f);
  for (i=0; i<256; i++) GTable[i] = (int)(greenTable[i] * 65535.0f + 0.5f);
  for (i=0; i<256; i++) BTable[i] = (int)(blueTable[i]  * 65535.0f + 0.5f);
  
  // Set new gammaTable:
  PsychGetCGDisplayIDFromScreenNumber(&cgDisplayID, screenNumber);
  XF86VidModeSetGammaRamp(cgDisplayID, PsychGetXScreenIdForScreen(screenNumber), 256, (unsigned short*) RTable, (unsigned short*) GTable, (unsigned short*) BTable);
#endif

  return;
}

// PsychGetDisplayBeamPosition() contains the implementation of display beamposition queries.
// It requires both, a cgDisplayID handle, and a logical screenNumber and uses one of both for
// deciding which display pipe to query, whatever of both is more efficient or suitable for the
// host platform -- This is ugly, but neccessary, because the mapping with only one of these
// specifiers would be either ambigous (wrong results!) or usage would be inefficient and slow
// (bad for such a time critical low level call!). On some systems it may even ignore the arguments,
// because it's not capable of querying different pipes - ie., it will always query a hard-coded pipe.
//
int PsychGetDisplayBeamPosition(CGDirectDisplayID cgDisplayId, int screenNumber)
{
  // Beamposition queries aren't supported by the X11 graphics system.
  // However, for gfx-hardware where we have reliable register specs, we
  // can do it ourselves, bypassing the X server.

  // On systems that we can't handle, we return -1 as an indicator
  // to high-level routines that we don't know the rasterbeam position.
  int beampos = -1;
  
  // Currently we can do do-it-yourself-style beamposition queries for
  // ATI's Radeon X1000, HD2000/3000 series chips due to availability of
  // hardware register specs. See top of this file for the setup and
  // shutdown code for the memory mapped access mechanism.
  if (gfx_cntl_mem) {
	  // Ok, supported chip and setup worked. Read the mmapped register,
	  // either for CRTC-1 if pipe for this screen is zero, or CRTC-2 otherwise:
	  beampos = radeon_get((displayScreensToPipes[screenNumber] == 0) ? RADEON_D1CRTC_STATUS_POSITION : RADEON_D2CRTC_STATUS_POSITION) & RADEON_VBEAMPOSITION_BITMASK;
  }

  // Return our result or non-result:
  return(beampos);
}

// Try to attach to kernel level ptb support driver and setup everything, if it works:
void InitPsychtoolboxKernelDriverInterface(void)
{
	// This is currently a no-op on Linux, as most low-level stuff is done via mmapped() MMIO access...
	return;
}

// Try to detach to kernel level ptb support driver and tear down everything:
void PsychOSShutdownPsychtoolboxKernelDriverInterface(void)
{
	if (numKernelDrivers > 0) {
		// Nothing to do yet...
	}

	// Ok, whatever happened, we're detached (for good or bad):
	numKernelDrivers = 0;

	return;
}

boolean PsychOSIsKernelDriverAvailable(int screenId)
{
	// Currently our "kernel driver" is available if MMIO mem could be mapped:
	// A real driver would indicate its presence via numKernelDrivers > 0 (see init/teardown code just above this routine):
	return((gfx_cntl_mem) ? TRUE : FALSE);
}

int PsychOSCheckKDAvailable(int screenId, unsigned int * status)
{
	// This doesn't make much sense on Linux yet. 'connect' should be something like a handle
	// to a kernel driver connection, e.g., the filedescriptor fd of the devicefile for ioctl()s
	// but we don't have such a thing yet.  Could be also a pointer to a little struct with all
	// relevant info...
	// Currently we do a dummy assignment...
	int connect = displayScreensToPipes[screenId];

	if ((numKernelDrivers<=0) && (gfx_cntl_mem == NULL)) {
		if (status) *status = ENODEV;
		return(0);
	}
	
	if (connect == 0xff) {
		if (status) *status = ENODEV;
		if (PsychPrefStateGet_Verbosity() > 6) printf("PTB-DEBUGINFO: Could not access kernel driver connection for screenId %i - No such connection.\n", screenId);
		return(0);
	}

	if (status) *status = 0;

	// Force this to '1', so the truth value is non-zero aka TRUE.
	connect = 1;
	return(connect);
}


unsigned int PsychOSKDReadRegister(int screenId, unsigned int offset, unsigned int* status)
{
	// Check availability of connection:
	int connect;
	if (!(connect = PsychOSCheckKDAvailable(screenId, status))) return(0xffffffff);
	if (status) *status = 0;

	// Return readback register value:
	return(radeon_get(offset));
}

unsigned int PsychOSKDWriteRegister(int screenId, unsigned int offset, unsigned int value, unsigned int* status)
{
	// Check availability of connection:
	int connect;
	if (!(connect = PsychOSCheckKDAvailable(screenId, status))) return(0xffffffff);
	if (status) *status = 0;

	// Write the register:
	radeon_set(offset, value);
	
	// Return success:
	return(0);
}

// Synchronize display screens video refresh cycle. See PsychSynchronizeDisplayScreens() for help and details...
PsychError PsychOSSynchronizeDisplayScreens(int *numScreens, int* screenIds, int* residuals, unsigned int syncMethod, double syncTimeOut, int allowedResidual)
{
	int screenId = 0;
	double	abortTimeOut, now;
	int residual;
	
	// Check availability of connection:
	int connect;
	unsigned int status;
	
	// No support for other methods than fast hard sync:
	if (syncMethod > 1) {
		if (PsychPrefStateGet_Verbosity() > 1) printf("PTB-WARNING: Could not execute display resync operation with requested non hard sync method. Not supported for this setup and settings.\n"); 
		return(PsychError_unimplemented);
	}
	
	// The current implementation only supports syncing all heads of a single card
	if (*numScreens <= 0) {
		// Resync all displays requested: Choose screenID zero for connect handle:
		screenId = 0;
	}
	else {
		// Resync of specific display requested: We only support resync of all heads of a single multi-head card,
		// therefore just choose the screenId of the passed master-screen for resync handle:
		screenId = screenIds[0];
	}
	
	if (!(connect = PsychOSCheckKDAvailable(screenId, &status))) {
		if (PsychPrefStateGet_Verbosity() > 1) printf("PTB-WARNING: Could not execute display resync operation for master screenId %i. Not supported for this setup and settings.\n", screenId); 
		return(PsychError_unimplemented);
	}
	
	// Setup deadline for abortion or repeated retries:
	PsychGetAdjustedPrecisionTimerSeconds(&abortTimeOut);
	abortTimeOut+=syncTimeOut;
	residual = INT_MAX;
	
	// Repeat until timeout or good enough result:
	do {
		// If this isn't the first try, wait 0.5 secs before retry:
		if (residual != INT_MAX) PsychWaitIntervalSeconds(0.5);
		
		residual = INT_MAX;

		// No op for now...		

		// Make it always TRUE == Success as this is a no op for now anyway...
		if (TRUE) {
			residual = (int) 0;
			if (PsychPrefStateGet_Verbosity() > 2) printf("PTB-INFO: Graphics display heads resynchronized. Residual vertical beamposition error is %ld scanlines.\n", residual);
		}
		else {
			if (PsychPrefStateGet_Verbosity() > 0) printf("PTB-ERROR: Graphics display head synchronization failed.\n");
			break;
		}
		
		// Timestamp:
		PsychGetAdjustedPrecisionTimerSeconds(&now);
	} while ((now < abortTimeOut) && (abs(residual) > allowedResidual));

	// Return residual value if wanted:
	if (residuals) { 
		residuals[0] = residual;
	}
	
	if (abs(residual) > allowedResidual) {
		if (PsychPrefStateGet_Verbosity() > 1) printf("PTB-WARNING: Failed to synchronize heads down to the allowable residual of +/- %i scanlines. Final residual %i lines.\n", allowedResidual, residual);
	}
	
	// TODO: Error handling not really worked out...
	if (residual == INT_MAX) return(PsychError_system);
	
	// Done.
	return(PsychError_none);
}

int PsychOSKDGetBeamposition(int screenId)
{
	// No-Op: This is implemented in PsychGetBeamposition() above directly...
	return(-1);
}