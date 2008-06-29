/*
	PsychToolbox3/Source/Common/PsychPortAudio/PsychPortAudio.c
	
	PLATFORMS:	All

	AUTHORS:
	Mario Kleiner   mk      mario.kleiner at tuebingen.mpg.de
	
	HISTORY:
	21.03.07		mk		wrote it.  
	
	DESCRIPTION:
	
	Low level Psychtoolbox sound i/o driver. Useful for audio capture, playback and
	feedback with well controlled timing and low latency. Uses the free software
	PortAudio library, API Version 19 (http://www.portaudio.com), which is not GPL,
	but has a more liberal and GPL compatible MIT style license.
	
	This seems to link statically against libportaudio.a on OS/X. However, it doesn't!
	Due to some restrictions of the OS/X linker we can't link statically against portaudio,
	so we have to have a .dylib version of the library installed in a system library
	search path, both for compiling and using PsychPortAudio. The current universal
	binary version of libportaudio.a for OS/X can be found in the...
	PsychSourceGL/Cohorts/PortAudio/ subfolder, which also contains versions for
	Linux and Windows.
	
*/


#include "PsychPortAudio.h"

#if PSYCH_SYSTEM == PSYCH_OSX
#include "pa_mac_core.h"
#endif

#if PSYCH_SYSTEM == PSYCH_WINDOWS
#include "pa_asio.h"
#endif

#define MAX_SYNOPSIS_STRINGS 50  

//declare variables local to this file.  
static const char *synopsisSYNOPSIS[MAX_SYNOPSIS_STRINGS];

#define kPortAudioPlayBack 1
#define kPortAudioCapture  2
#define kPortAudioFullDuplex 3
#define kPortAudioMonitoring 4

// Maximum number of audio devices we handle:
#define MAX_PSYCH_AUDIO_DEVS 10

// Maximum number of audio channels we support per open device:
#define MAX_PSYCH_AUDIO_CHANNELS_PER_DEVICE 256

// Our device record:
typedef struct PsychPADevice {
	int		 opmode;			// Mode of operation: Playback, capture or full duplex?
	PaStream *stream;			// Pointer to associated portaudio stream.
	PaStreamInfo* streaminfo;   // Pointer to stream info structure, provided by PortAudio.
	PaHostApiTypeId hostAPI;	// Type of host API.
	double	 startTime;			// Requested start time in system time (secs). Returns real start time after start.
								// The real start time is the time when the first sample hit the speaker in playback or full-duplex mode.
								// Its the time when the first sample was captured in pure capture mode - or when the first sample should
								// be captured in pure capture mode with scheduled start. Whenever playback is active, startTime only
								// refers to the output stage.
	double	 captureStartTime;	// Time when first captured sample entered the sound hardware in system time (secs). This information is
								// redundant in pure capture mode - its identical to startTime. In full duplex mode, this is an estimate of
								// when the first sample was captured, whereas startTime is an estimate of when the first sample was output.
	int		 state;				// Current state of the stream: 0=Stopped, 1=Hot Standby, 2=Playing, 3=Aborting playback.
	int		 repeatCount;		// Number of repetitions: -1 = Loop forever, 1 = Once, n = n repetitions.
	float*	 outputbuffer;		// Pointer to float memory buffer with sound output data.
	int		 outputbuffersize;	// Size of output buffer in bytes.
	unsigned int playposition;	// Current playposition in samples since start of playback (not frames, not bytes!)
	unsigned int writeposition; // Current writeposition in samples since start of playback (for incremental filling).
	float*	 inputbuffer;		// Pointer to float memory buffer with sound input data (captured sound data).
	int		 inputbuffersize;	// Size of input buffer in bytes.
	unsigned int recposition;	// Current record position in samples since start of capture.
	unsigned int readposition;  // Last read-out sample since start of capture.
	unsigned int outchannels;	// Number of output channels.
	unsigned int inchannels;	// Number of input channels.
	unsigned int xruns;			// Number of over-/underflows of input-/output channel for this stream.
	unsigned int paCalls;		// Number of callback invocations.
	unsigned int noTime;		// Number of timestamp malfunction - Should not happen anymore.
	unsigned int batchsize;		// Maximum number of frames requested during callback invokation: Estimate of real buffersize.
	double	 predictedLatency;  // Latency that PortAudio predicts for current callbackinvocation. We will compensate for that when starting audio.
	double   latencyBias;		// A bias value to add to the value that PortAudio reports for total buffer->Speaker latency.
								// This value defaults to zero, but can be set up automatically on OS/X or manually on other OSes to compensate
								// for slight mistakes in PA's estimate.
} PsychPADevice;

PsychPADevice audiodevices[MAX_PSYCH_AUDIO_DEVS];
unsigned int  audiodevicecount;
unsigned int  verbosity = 4;

boolean pa_initialized = FALSE;

/* Logger callback function to output PortAudio debug messages at 'verbosity' > 5. */
void PALogger(const char* msg)
{
	if (verbosity > 5) printf("PTB-DEBUG: PortAudio says: %s", msg);
	return;
}


/* paCallback: PortAudo I/O processing callback. 
 *
 * This callback is called by PortAudios playback/capture engine whenever
 * it needs new data for playback or has new data from capture. We are expected
 * to take the inputBuffer's content and store it in our own recording buffers,
 * and push data from our playback buffer queue into the outputBuffer.
 *
 * timeInfo tells us useful timing information, so we can estimate latencies,
 * compensate for them, and so on...
 *
 * This callback is part of a realtime/interrupt/system context so don't do
 * things like calling PortAudio functions, allocating memory, file i/o or
 * other unbounded operations!
 */
static int paCallback( const void *inputBuffer, void *outputBuffer,
                             unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo* timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void *userData )
{
	// Assign all variables, especially our dev device structure
	// with info about this stream:
    PsychPADevice* dev = (PsychPADevice*) userData;
    float *out = (float*) outputBuffer;
    float *in = (float*) inputBuffer;
    unsigned long i, silenceframes;
	unsigned long inchannels, outchannels;
	unsigned int  playposition, outsbsize, insbsize, recposition;
	double now, firstsampleonset, onsetDelta;
	int repeatCount;
	PaHostApiTypeId hA;

	// Sound buffer attached to stream? If no sound buffer
	// is attached, we can't continue and tell the engine to abort
	// processing of this stream:
	if (dev == NULL) return(paAbort);
	
	// Check if all required buffers are there. In monitoring mode, we don't need them.
    if ((dev->opmode & kPortAudioMonitoring == 0) && (((dev->opmode & kPortAudioPlayBack) && (dev->outputbuffer == NULL)) ||
		((dev->opmode & kPortAudioCapture) && (dev->inputbuffer == NULL)))) return(paAbort);

	// Count total number of calls:
	dev->paCalls++;
	
	// Call number of timestamp failures:
	if (timeInfo->currentTime == 0) dev->noTime++;
	
	// Keep track of maximum number of frames requested:
	if (dev->batchsize < framesPerBuffer) dev->batchsize = framesPerBuffer;
	
	// Keep track of buffer over-/underflows:
	if (statusFlags & (paInputOverflow | paInputUnderflow | paOutputOverflow | paOutputUnderflow)) dev->xruns++;

	// Query number of output channels:
	outchannels = (unsigned long) dev->outchannels;

	// Query number of output channels:
	inchannels = (unsigned long) dev->inchannels;
	
	// Query number of repetitions:
	repeatCount = dev->repeatCount;
	
	// Get our current playback position in samples (not frames or bytes!!):
	playposition = dev->playposition;
	recposition = dev->recposition;
	
	// Compute size of soundbuffers in samples:
	outsbsize = dev->outputbuffersize / sizeof(float);
	insbsize = dev->inputbuffersize / sizeof(float);

	// Logical playback state is "stopped" or "aborting" ? If so, abort.
	if ((dev->state == 0) || (dev->state == 3)) {
		// Prime the outputbuffer with silence to avoid ugly noise.
		if (outputBuffer) memset(outputBuffer, 0, framesPerBuffer * outchannels * sizeof(float));
		return(paComplete);
	}

	// Are we already playing back and/or capturing real audio data,
	// or are we still on hot-standby? PsychPortAudio tries to start
	// playback/capture of a sound exactly at a requested point in
	// system time (i.e. the first sound sample should hit the speaker
	// as closely as possible to that point in time) by use of the
	// following trick: When the user script executes PsychPortAudio('Start'),
	// our routine immediately starts processing of the portaudio stream,
	// starting up the audio hardwares DACs/ADCs and Portaudios engine.
	// After a short latency, our paCallback() (this routine) gets called
	// by the realtime audio scheduler, requesting or providing audio sample
	// data. In the provided timeInfo - variable, we are provided with the
	// current playback time of the audio device (in seconds since stream start)
	// and an estimate of when our first provided sample will hit the speakers.
	// We convert these timestamps into system time, so we'll know how far the user
	// provided onset deadline is away. If the deadline is far away, so the samples
	// for this callback iteration would hit the speaker too early, we simply return
	// a zero filled outputbuffer --> We output silence. If the deadline is somewhere
	// in the middle of this outputbuffer, we fill the appropriate amount of bufferspace
	// with zeros, then copy in our first real samples into the remaining buffer.
	// After that, we switch to real playback, all future calls will provide PA
	// with real sampledata. Assuming the sound onset estimate provided by PA is
	// correct, this should allow accurate sound onset. It all depends on the
	// latency estimate...
	hA=dev->hostAPI;
	
	// Hot standby?
	if (dev->state == 1) {
		// Hot standby: Query and convert timestamps to system time.
		#if PSYCH_SYSTEM == PSYCH_WINDOWS
			// Super ugly hack for the most broken system in existence: Force
			// the audio thread to cpu core 1, hope that repeated redundant
			// calls don't create scheduling overhead or excessive other overhead.
			// The explanation for this can be found in Source/Windows/Base/PsychTimeGlue.c
			SetThreadAffinityMask(GetCurrentThread(), 1);
		#endif
		
		// Retrieve current system time:
		PsychGetAdjustedPrecisionTimerSeconds(&now);
		
		// FIXME: PortAudio stable sets timeInfo->currentTime == 0 --> Breakage!!!
		// That's why we currently have our own PortAudio version.
		
		if (hA==paCoreAudio || hA==paDirectSound || hA==paMME || hA==paALSA) {
			// On these systems, DAC-time is already returned in the system timebase,
			// at least with our modified version of PortAudio, so a simple
			// query will return the onset time of the first sample. Well,
			// looks as if we need to add the device inherent latency, because
			// this describes everything up to the point where DMA transfer is
			// initiated, but not device inherent latency. This additional latency
			// is added via latencyBias, which is initialized in the Open-Function
			// via a low-level driver query to CoreAudio:
			if (dev->opmode & kPortAudioPlayBack) {
				// Playback enabled: Use DAC time as basis for timing:
				firstsampleonset = (double) timeInfo->outputBufferDacTime + dev->latencyBias;
			}
			else {
				// Recording (only): Use ADC time as basis for timing:
				firstsampleonset = (double) timeInfo->inputBufferAdcTime + dev->latencyBias;
			}
			
			// Store our measured PortAudio + CoreAudio + Driver + Hardware latency:
			dev->predictedLatency = firstsampleonset - now;
			
			if (dev->opmode & kPortAudioCapture) {
				// Store estimated capturetime in captureStartTime. This is only important in
				// full-duplex mode, redundant in pure half-duplex capture mode:
				dev->captureStartTime = (double) timeInfo->inputBufferAdcTime;
			}
		}
		else {
			// ASIO or unknown. ASIO needs to be checked, which category is correct.
			// Not yet verified how these other audio APIs behave. Play safe
			// and perform timebase remapping: This also needs our special fixed
			// PortAudio version where currentTime actually has a value:
			if (dev->opmode & kPortAudioPlayBack) {
				// Playback enabled: Use DAC time as basis for timing:
				firstsampleonset = ((double) (timeInfo->outputBufferDacTime - timeInfo->currentTime));
			}
			else {
				// Recording (only): Use ADC time as basis for timing:
				firstsampleonset = ((double) (timeInfo->inputBufferAdcTime - timeInfo->currentTime));
			}
			
			// Store our measured PortAudio + HostAPI + Driver + Hardware latency:
			dev->predictedLatency = firstsampleonset + dev->latencyBias;

			// Assign predicted (remapped to our time system) audio onset time for this buffer:
			firstsampleonset = now + firstsampleonset + dev->latencyBias;
			
			if (dev->opmode & kPortAudioCapture) {
				// Store estimated capturetime in captureStartTime. This is only important in
				// full-duplex mode, redundant in pure half-duplex capture mode:
				dev->captureStartTime = now + ((double) (timeInfo->inputBufferAdcTime - timeInfo->currentTime));
			}
		}
		
		if (FALSE) {
			// Debug code to compare our two timebases against each other: On OS/X,
			// luckily both timebases are identical, ie. our UpTime() timebase used
			// everywhere in PTB and the CoreAudio AudioHostClock() are identical.
			psych_uint64 ticks;
			double  tickssec;
			PsychGetPrecisionTimerTicksPerSecond(&tickssec);
			PsychGetPrecisionTimerTicks(&ticks);
			printf("AudioHostClock: %lf   vs. System clock: %lf\n", ((double) ticks) / tickssec, now); 
		}
		
		// Compute difference between requested onset time and presentation time
		// of the first sample of this callbacks returned buffer:
		onsetDelta = dev->startTime - firstsampleonset;
		
		// Time left until onset and in playback mode?
		if ((onsetDelta > 0) && (dev->opmode & kPortAudioPlayBack)) {
			// Some time left: A full buffer duration?
			if (onsetDelta >= ((double) framesPerBuffer / (double) dev->streaminfo->sampleRate)) {
				// At least one buffer away. Fill our buffer with zeros, aka silence:
				memset(outputBuffer, 0, framesPerBuffer * outchannels * sizeof(float));
				
				// Ready. Tell engine to continue stream processing, i.e., call us again...
				return(paContinue);
			}
			else {
				// A bit time left, but less than a full buffer. Need to pad the head of
				// this buffer with zeros, aka silence, then fill the rest with real data:
				silenceframes = (unsigned long) (onsetDelta * ((double) dev->streaminfo->sampleRate));

				// Fill in some silence:
				memset(outputBuffer, 0, silenceframes * outchannels * sizeof(float));
				out+= (silenceframes * outchannels);

				// Decrement remaining real audio data count:
				framesPerBuffer-=silenceframes;

				// dev->StartTime now exactly corresponds to onset of first non-silence sample.
				// At least if everything works properly.

				// Mark us as running:
				dev->state = 2;
			}
		}
		else {
			// Ooops! We are late! Store real estimated onset in the startTime field and
			// then hurry up! If we are in "capture only" mode, we disregard the 'when'
			// deadline and always start immediately.
			dev->startTime = firstsampleonset;

			// Mark us as running:
			dev->state = 2;
		}
	}
    
	// This code only executes in live monitoring mode - a mode where all
	// sound data is fed back immediately with shortest possible latency
	// from input to output, without any involvement of Matlab/Octave code:
	if (dev->opmode & kPortAudioMonitoring) {
		// Copy input buffer to output buffer:
		memcpy(out, in, framesPerBuffer * outchannels * sizeof(float));
		
		// Store updated positions in device structure:
		dev->playposition = playposition + (framesPerBuffer * outchannels);
		dev->recposition = dev->playposition;
		
		// Return from callback:
		return(paContinue);
	}
	
	// This code retrieves and stores captured sound data, if any:
	if (dev->opmode & kPortAudioCapture) {
		// This is the simple case (compared to playback processing).
		// Just copy all available data to our internal buffer:
		for (i=0; (i < framesPerBuffer * inchannels); i++) {
			dev->inputbuffer[recposition % insbsize] = (float) *in++;
			recposition++;
		}
		
		// Store updated recording position in device structure:
		dev->recposition = recposition;
	}
	
	// This code emits actual sound data to the engine:
	if (dev->opmode & kPortAudioPlayBack) {
		// Copy requested number of samples for each channel into the output buffer: Take the case of
		// "loop forever" and "loop repeatCount" times into account:
		for (i=0; (i < framesPerBuffer * outchannels) && ((repeatCount == -1) || (playposition < (repeatCount * outsbsize))); i++) {
			*out++ = dev->outputbuffer[playposition % outsbsize];
			playposition++;
		}
		
		// Store updated playposition in device structure:
		dev->playposition = playposition;
		
		// End of playback reached due to maximum repeatCount reached?
		if (i < framesPerBuffer * outchannels) {
			// Premature stop of buffer filling because repeatCount exceeded.
			// We need to zero-fill the remainder of the buffer and tell the engine
			// to finish playback:
			while(i < framesPerBuffer * outchannels) {
				*out++ = 0.0;
				i++;
			}
			
			// Signal that the filled buffers up to this point should be played,
			// but after that the engine should stop the stream:
			return(paComplete);
		}
	}
		
	// Tell engine to continue stream processing, i.e., call us again...
    return(paContinue);
}

void PsychPACloseStream(int id)
{
	PaStream* stream = audiodevices[id].stream;
	
	if (stream) {
		// Stop, shutdown and release audio stream:
		Pa_StopStream(stream);
		Pa_CloseStream(stream);
		audiodevices[id].stream = NULL;
		
		// Free associated sound outputbuffer:
		if(audiodevices[id].outputbuffer) {
			free(audiodevices[id].outputbuffer);
			audiodevices[id].outputbuffer = NULL;
			audiodevices[id].outputbuffersize = 0;
		}				

		// Free associated sound inputbuffer:
		if(audiodevices[id].inputbuffer) {
			free(audiodevices[id].inputbuffer);
			audiodevices[id].inputbuffer = NULL;
			audiodevices[id].inputbuffersize = 0;
		}				

		audiodevicecount--;
	}
	
	return;
}

void InitializeSynopsis()
{
	int i=0;
	const char **synopsis = synopsisSYNOPSIS;  //abbreviate the long name
	
	synopsis[i++] = "PsychPortAudio - A sound driver built around the PortAudio sound library:\n";
	synopsis[i++] = "\nGeneral information:\n";
	synopsis[i++] = "version = PsychPortAudio('Version');";
	synopsis[i++] = "oldlevel = PsychPortAudio('Verbosity' [,level]);";
	synopsis[i++] = "devices = PsychPortAudio('GetDevices' [,devicetype]);";
	synopsis[i++] = "status = PsychPortAudio('GetStatus' pahandle);";
	synopsis[i++] = "\n\nDevice setup and shutdown:\n";
	synopsis[i++] = "pahandle = PsychPortAudio('Open' [, deviceid][, mode][, reqlatencyclass][, freq][, channels][, buffersize][, suggestedLatency][, selectchannels]);";
	synopsis[i++] = "PsychPortAudio('Close' [, pahandle]);";
	synopsis[i++] = "oldbias = PsychPortAudio('LatencyBias', pahandle [,biasSecs]);";
	synopsis[i++] = "PsychPortAudio('FillBuffer', pahandle, bufferdata [, streamingrefill=0);";
	synopsis[i++] = "startTime = PsychPortAudio('Start', pahandle [, repetitions=1] [, when=0] [, waitForStart=0] );";
	synopsis[i++] = "startTime = PsychPortAudio('RescheduleStart', pahandle, when [, waitForStart=0]);";
	synopsis[i++] = "[startTime endPositionSecs xruns] = PsychPortAudio('Stop', pahandle [,waitForEndOfPlayback=0]);";
	synopsis[i++] = "[audiodata absrecposition overflow cstarttime] = PsychPortAudio('GetAudioData', pahandle [, amountToAllocateSecs][, minimumAmountToReturnSecs][, maximumAmountToReturnSecs]);";
	
	synopsis[i++] = NULL;  //this tells PsychDisplayScreenSynopsis where to stop
	if (i > MAX_SYNOPSIS_STRINGS) {
		PrintfExit("%s: increase dimension of synopsis[] from %ld to at least %ld and recompile.",__FILE__,(long)MAX_SYNOPSIS_STRINGS,(long)i);
	}
}

PaHostApiIndex PsychPAGetLowestLatencyHostAPI(void)
{
	PaHostApiIndex ai;
	
	#if PSYCH_SYSTEM == PSYCH_OSX
		// CoreAudio or nothing ;-)
		return(Pa_HostApiTypeIdToHostApiIndex(paCoreAudio));
	#endif
	
	#if PSYCH_SYSTEM == PSYCH_LINUX
		// Try ALSA first...
		if (((ai=Pa_HostApiTypeIdToHostApiIndex(paALSA))!=paHostApiNotFound) && (Pa_GetHostApiInfo(ai)->deviceCount > 0)) return(ai);
		// Then JACK...
		if (((ai=Pa_HostApiTypeIdToHostApiIndex(paJACK))!=paHostApiNotFound) && (Pa_GetHostApiInfo(ai)->deviceCount > 0)) return(ai);
		// Then OSS...
		if (((ai=Pa_HostApiTypeIdToHostApiIndex(paOSS))!=paHostApiNotFound) && (Pa_GetHostApiInfo(ai)->deviceCount > 0)) return(ai);
		// then give up!
		printf("PTB-ERROR: Could not find an operational audio subsystem on this Linux machine! Soundcard and driver installed and enabled?!?\n");
		return(paHostApiNotFound);
	#endif
	
	#if PSYCH_SYSTEM == PSYCH_WINDOWS
		// Try ASIO first. It's supposed to be the lowest latency API on soundcards that suppport it.
		if (((ai=Pa_HostApiTypeIdToHostApiIndex(paASIO))!=paHostApiNotFound) && (Pa_GetHostApiInfo(ai)->deviceCount > 0)) return(ai);
		// Then WDM kernel streaming (Win2000, XP, maybe Vista). This is the best working free builtin
		// replacement for the proprietary ASIO interface.
		if (((ai=Pa_HostApiTypeIdToHostApiIndex(paWDMKS))!=paHostApiNotFound) && (Pa_GetHostApiInfo(ai)->deviceCount > 0)) return(ai);
		// Then Vistas new WASAPI, which is supposed to replace WDMKS, but is still early alpha quality in PortAudio:
		if (((ai=Pa_HostApiTypeIdToHostApiIndex(paWASAPI))!=paHostApiNotFound) && (Pa_GetHostApiInfo(ai)->deviceCount > 0)) return(ai);
		// Then DirectSound: Bad, but not a complete disaster if the sound card has DS native drivers: 
		if (((ai=Pa_HostApiTypeIdToHostApiIndex(paDirectSound))!=paHostApiNotFound) && (Pa_GetHostApiInfo(ai)->deviceCount > 0)) return(ai);
		// Then Windows MME, a complete disaster, but better than silence...?!? 
		if (((ai=Pa_HostApiTypeIdToHostApiIndex(paMME))!=paHostApiNotFound) && (Pa_GetHostApiInfo(ai)->deviceCount > 0)) return(ai);
		// then give up!
		printf("PTB-ERROR: Could not find an operational audio subsystem on this Windows machine! Soundcard and driver installed and enabled?!?\n");
		return(paHostApiNotFound);
	#endif
	
	printf("PTB-FATAL-ERROR: Impossible point in code execution reached! (End of PsychPAGetLowestLatencyHostAPI()\n");
	return(paHostApiNotFound);
}

PsychError PSYCHPORTAUDIODisplaySynopsis(void)
{
	int i;
	
	for (i = 0; synopsisSYNOPSIS[i] != NULL; i++)
		printf("%s\n",synopsisSYNOPSIS[i]);
	
	return(PsychError_none);
}

// Module exit function: Stop and close all audio streams, terminate PortAudio...
PsychError PsychPortAudioExit(void)
{
	PaError err;
	int i;
	
	if (pa_initialized) {
		for(i=0; i<MAX_PSYCH_AUDIO_DEVS; i++) {
			// Close i'th stream, if it is open:
			PsychPACloseStream(i);
		}
		audiodevicecount = 0;
		
		// Shutdown PortAudio itself:
		err = Pa_Terminate();
		if (err) {
			printf("PTB-FATAL-ERROR: PsychPortAudio: Shutdown of PortAudio subsystem failed. Depending on the quality\n");
			printf("PTB-FATAL-ERROR: of your operating system, this may leave the sound system of your machine dead or confused.\n");
			printf("PTB-FATAL-ERROR: Exit and restart Matlab/Octave. Windows users additionally may want to reboot...\n");
			printf("PTB-FATAL-ERRRO: PortAudio reported the following error: %s\n\n", Pa_GetErrorText(err)); 
		}
		else {
			pa_initialized = FALSE;
		}
	}

	// Detach our callback function for low-level debug output:
	PaUtil_SetDebugPrintFunction(NULL);
	
	return(PsychError_none);
}

void PsychPortAudioInitialize(void)
{
	PaError err;
	int i;
	
	// PortAudio already initialized?
	if (!pa_initialized) {
		// Setup callback function for low-level debug output:
		PaUtil_SetDebugPrintFunction(PALogger);

		if ((err=Pa_Initialize())!=paNoError) {
			printf("PTB-ERROR: Portaudio initialization failed with following port audio error: %s \n", Pa_GetErrorText(err));
			PsychErrorExitMsg(PsychError_system, "Failed to initialize PortAudio subsystem.");
		}
		else {
			if(verbosity>2) printf("PTB-INFO: Using specially modified PortAudio engine, based on offical version: %s\n", Pa_GetVersionText());
		}
		
		for(i=0; i<MAX_PSYCH_AUDIO_DEVS; i++) {
			audiodevices[i].stream = NULL;
		}
		
		audiodevicecount=0;

		pa_initialized = TRUE;
	}
}

/* PsychPortAudio('Open') - Open and initialize an audio device via PortAudio.
 */
PsychError PSYCHPORTAUDIOOpen(void) 
{
 	static char useString[] = "pahandle = PsychPortAudio('Open' [, deviceid][, mode][, reqlatencyclass][, freq][, channels][, buffersize][, suggestedLatency][, selectchannels]);";
	//															1			 2		 3					4		5			6			  7					  8
	static char synopsisString[] = 
		"Open a PortAudio audio device and initialize it. Returns a 'pahandle' device handle for the device. "
		"All parameters are optional and have reasonable defaults. 'deviceid' Index to select amongst multiple "
		"logical audio devices supported by PortAudio. Defaults to whatever the systems default sound device is. "
		"Different device id's may select the same physical device, but controlled by a different low-level sound "
		"system. E.g., Windows has about five different sound subsystems. 'mode' Mode of operation. Defaults to "
		"1 == sound playback only. Can be set to 2 == audio capture, or 3 for simultaneous capture and playback of sound. "
		"Note however that mode 3 (full duplex) does not work reliably on all sound hardware. On some hardware this mode "
		"may crash Matlab! \n"
		"'reqlatencyclass' Allows to select how aggressive PsychPortAudio should be about minimizing sound latency "
		"and getting good deterministic timing, i.e. how to trade off latency vs. system load and playing nicely "
		"with other sound applications on the system. Level 0 means: Don't care about latency, this mode works always "
		"and with all settings, plays nicely with other sound applications. Level 1 (the default) means: Try to get "
		"the lowest latency that is possible under the constraint of reliable playback, freedom of choice for all parameters and "
		"interoperability with other applications. Level 2 means: Take full control over the audio device, even if this "
		"causes other sound applications to fail or shutdown. Level 3 means: As level 2, but request the most aggressive "
		"settings for the given device. Level 4: Same as 3, but fail if device can't meet the strictest requirements. "
		"'freq' Requested playback/capture rate in samples per second (Hz). Defaults to a value that depends on the "
		"requested latency mode. 'channels' Number of audio channels to use, defaults to 2 for stereo. If you perform "
		"simultaneous playback and recording, you can provide a 2 element vector for 'channels', specifying different "
		"numbers of output channels and input channels. The first element in such a vector defines the number of playback "
		"channels, the 2nd element defines capture channels. E.g., [2, 1] would define 2 playback channels (stereo) and 1 "
		"recording channel. See the optional 'selectchannels' argument for selection of physical device channels on multi- "
		"channel cards.\n"
		"'buffersize' "
		"requested size and number of internal audio buffers, smaller numbers mean lower latency but higher system load "
		"and some risk of overloading, which would cause audio dropouts. 'suggestedLatency' optional requested latency in "
		"seconds. PortAudio selects internal operating parameters depending on sampleRate, suggestedLatency and buffersize "
		"as well as device internal properties to optimize for low latency output. Best left alone, only here as manual "
		"override in case all the auto-tuning cleverness fails.\n "
		"'selectchannels' optional matrix with mappings of logical channels to device channels: If you only want to use "
		"a subset of the channels present on your sound card, e.g., only 2 playback channels on a 16 channel soundcard, "
		"then you'd set the 'channels' argument to 2. The 'selectchannels' argument allows you to select, e.g.,  which "
		"two of the 16 channels to use for playback. 'selectchannels' is a one row by 'channels' matrix with mappings "
		"for pure playback or pure capture. For full-duplex mode (playback and capture), 'selectchannels' must be a "
		"2 rows by max(channels) column matrix. row 1 will define playback channel mappings, whereas row 2 will then "
		"define capture channel mappings. In any case, the number in the i'th column will define which physical device "
		"channel will be used for playback or capture of the i'th PsychPortAudio channel (the i'th row of your sound "
		"matrix). Numbering of physical device channels starts with zero! Example: Both, playback and simultaneous "
		"recording are requested and 'channels' equals 2, ie, two playback channels and two capture channels. If you'd "
		"specify 'selectchannels' as [0, 6 ; 12, 14], then playback would happen to device channels zero and six, "
		"sound would be captured from device channels 12 and 14. Please note that channel selection is currently "
		"only supported on MS-Windows with ASIO sound cards. The parameter is silently ignored for non-ASIO operation. ";
	
	static char seeAlsoString[] = "Close GetDeviceSettings ";	 
  	
	int freq, buffersize, latencyclass, mode, deviceid, i, numel;
	int* nrchannels;
	int  mynrchannels[2];
	int  m, n, p;
	double* mychannelmap;
	double suggestedLatency, lowlatency;
	PaDeviceIndex paDevice;
	PaHostApiIndex paHostAPI;
	PaStreamParameters outputParameters;
	PaStreamParameters inputParameters;
	PaDeviceInfo* inputDevInfo, *outputDevInfo, *referenceDevInfo;
	PaStreamFlags sflags;
	PaError err;
	PaStream *stream = NULL;
	
	#if PSYCH_SYSTEM == PSYCH_OSX
		paMacCoreStreamInfo hostapisettings;
	#endif
	
	#if PSYCH_SYSTEM == PSYCH_WINDOWS
		// Additional data structures for setup of logical -> device channel
		// mappings on ASIO multi-channel hardware:
		PaAsioStreamInfo  inhostapisettings;
		PaAsioStreamInfo  outhostapisettings;
		int				  outputmappings[MAX_PSYCH_AUDIO_CHANNELS_PER_DEVICE];
		int				  inputmappings[MAX_PSYCH_AUDIO_CHANNELS_PER_DEVICE];
	#endif
	
	// Setup online help: 
	PsychPushHelp(useString, synopsisString, seeAlsoString);
	if(PsychIsGiveHelp()) {PsychGiveHelp(); return(PsychError_none); };
	
	PsychErrorExit(PsychCapNumInputArgs(8));     // The maximum number of inputs
	PsychErrorExit(PsychRequireNumInputArgs(0)); // The required number of inputs	
	PsychErrorExit(PsychCapNumOutputArgs(1));	 // The maximum number of outputs

	if (audiodevicecount >= MAX_PSYCH_AUDIO_DEVS) PsychErrorExitMsg(PsychError_user, "Maximum number of simultaneously open audio devices reached.");
	 
	freq = 0;
	buffersize = 0;
	latencyclass = 1;
	mode = kPortAudioPlayBack;
	deviceid = -1;
	
	// Make sure PortAudio is online:
	PsychPortAudioInitialize();
	
	// Sanity check: Any hardware found?
	if (Pa_GetDeviceCount() == 0) PsychErrorExitMsg(PsychError_user, "Could not find *any* audio hardware on your system! Either your machine doesn't have audio hardware, or somethings seriously screwed.");
	
	// We default to generic system settings for host api specific settings:
	outputParameters.hostApiSpecificStreamInfo = NULL;
	inputParameters.hostApiSpecificStreamInfo  = NULL;

	// Request optional deviceid:
	PsychCopyInIntegerArg(1, kPsychArgOptional, &deviceid);
	if (deviceid < -1) PsychErrorExitMsg(PsychError_user, "Invalid deviceid provided. Valid values are -1 to maximum number of devices.");

	// Request optional mode of operation:
	PsychCopyInIntegerArg(2, kPsychArgOptional, &mode);
	if (mode < 1 || mode > 7) PsychErrorExitMsg(PsychError_user, "Invalid mode provided. Valid values are 1 to 7.");

	// Request optional latency class:
	PsychCopyInIntegerArg(3, kPsychArgOptional, &latencyclass);
	if (latencyclass < 0 || latencyclass > 4) PsychErrorExitMsg(PsychError_user, "Invalid reqlatencyclass provided. Valid values are 0 to 4.");

	if (deviceid == -1) {
		// Default devices requested:
		if (latencyclass == 0) {
			// High latency mode: Simply pick system default devices:
			outputParameters.device = Pa_GetDefaultOutputDevice(); /* Default output device. */
			inputParameters.device  = Pa_GetDefaultInputDevice(); /* Default input device. */
		}
		else {
			// Low latency mode: Try to find the host API which is supposed to be the fastest on
			// a platform, then pick its default devices:
			paHostAPI = PsychPAGetLowestLatencyHostAPI();
			outputParameters.device = Pa_GetHostApiInfo(paHostAPI)->defaultOutputDevice;
			inputParameters.device  = Pa_GetHostApiInfo(paHostAPI)->defaultInputDevice;
		}
	}
	else {
		// Specific device requested: In valid range?
		if (deviceid >= Pa_GetDeviceCount() || deviceid < 0) {
			PsychErrorExitMsg(PsychError_user, "Invalid deviceid provided. Higher than the number of devices - 1 or lower than zero.");
		}
		
		outputParameters.device = (PaDeviceIndex) deviceid;
		inputParameters.device = (PaDeviceIndex) deviceid;
	}

	// Query properties of selected device(s):
	inputDevInfo  = Pa_GetDeviceInfo(inputParameters.device);
	outputDevInfo = Pa_GetDeviceInfo(outputParameters.device);

	// Select one of them as "reference" info devices: It's properties are used whenever
	// no more specialized info is available. We use the output device (if any) as reference,
	// otherwise fall back to the inputdevice.
	referenceDevInfo = (outputDevInfo) ? outputDevInfo : inputDevInfo;

	// Sanity check: Any hardware found?
	if (referenceDevInfo == NULL) PsychErrorExitMsg(PsychError_user, "Could not find *any* audio hardware on your system - or at least not with the provided deviceid, if any!");

	// Check if current set of selected/available devices is compatible with our playback mode:
	if (((mode & kPortAudioPlayBack) || (mode & kPortAudioMonitoring)) && ((outputDevInfo == NULL) || (outputDevInfo && outputDevInfo->maxOutputChannels <= 0))) {
		PsychErrorExitMsg(PsychError_user, "Audio output requested, but there isn't any audio output device available or you provided a deviceid for something else than an output device!");
	}

	if (((mode & kPortAudioCapture) || (mode & kPortAudioMonitoring)) && ((inputDevInfo == NULL) || (inputDevInfo && inputDevInfo->maxInputChannels <= 0))) {
		PsychErrorExitMsg(PsychError_user, "Audio input requested, but there isn't any audio input device available or you provided a deviceid for something else than an input device!");
	}
	
	// Request optional frequency:
	PsychCopyInIntegerArg(4, kPsychArgOptional, &freq);
	if (freq < 0 || freq > 200000) PsychErrorExitMsg(PsychError_user, "Invalid frequency provided. Valid values are 0 to 200000 Hz.");
	
	// Request optional number of channels:
	numel = 0; nrchannels = NULL;
	PsychAllocInIntegerListArg(5, kPsychArgOptional, &numel, &nrchannels);
	if (numel == 0) {
		// No optional channelcount argument provided: Default to two for playback and recording, unless device is
		// a mono device -- then we default to one:
		mynrchannels[0] = (outputDevInfo && outputDevInfo->maxOutputChannels < 2) ? outputDevInfo->maxOutputChannels : 2;
		mynrchannels[1] = (inputDevInfo && inputDevInfo->maxInputChannels < 2) ? inputDevInfo->maxInputChannels : 2;
	}
	else if (numel == 1) {
		// One argument provided: Set same count for playback and recording:
		if (*nrchannels < 1 || *nrchannels > MAX_PSYCH_AUDIO_CHANNELS_PER_DEVICE) PsychErrorExitMsg(PsychError_user, "Invalid number of channels provided. Valid values are 1 to device maximum.");
		mynrchannels[0] = *nrchannels;
		mynrchannels[1] =  *nrchannels;		
	}
	else if (numel == 2) {
		// Separate counts for playback and recording provided: Set'em up.
		if (nrchannels[0] < 1 || nrchannels[0] > MAX_PSYCH_AUDIO_CHANNELS_PER_DEVICE) PsychErrorExitMsg(PsychError_user, "Invalid number of playback channels provided. Valid values are 1 to device maximum.");
		mynrchannels[0] = nrchannels[0];
		if (nrchannels[1] < 1 || nrchannels[1] > MAX_PSYCH_AUDIO_CHANNELS_PER_DEVICE) PsychErrorExitMsg(PsychError_user, "Invalid number of capture channels provided. Valid values are 1 to device maximum.");
		mynrchannels[1] = nrchannels[1];
	}
	else {
		// More than 2 channel counts provided? Impossible.
		PsychErrorExitMsg(PsychError_user, "You specified a list with more than two 'channels' entries? Can only be max 2 for playback- and capture.");
	}

	// Make sure that number of capture and playback channels is the same for fast monitoring/feedback mode:
	if ((mode & kPortAudioMonitoring) && (mynrchannels[0] != mynrchannels[1])) PsychErrorExitMsg(PsychError_user, "Fast monitoring/feedback mode selected, but number of capture and playback channels differs! They must be the same for this mode!");

	// Request optional buffersize:
	PsychCopyInIntegerArg(6, kPsychArgOptional, &buffersize);
	if (buffersize < 0 || buffersize > 4096) PsychErrorExitMsg(PsychError_user, "Invalid buffersize provided. Valid values are 0 to 4096 samples.");

	// Request optional suggestedLatency:
	suggestedLatency = -1.0;
	PsychCopyInDoubleArg(7, kPsychArgOptional, &suggestedLatency);
	if (suggestedLatency!=-1 && (suggestedLatency < 0.0 || suggestedLatency > 1.0)) PsychErrorExitMsg(PsychError_user, "Invalid suggestedLatency provided. Valid values are 0.0 to 1.0 seconds.");
	
	// Get optional channel map:
	mychannelmap = NULL;
	PsychAllocInDoubleMatArg(8, kPsychArgOptional, &m, &n, &p, &mychannelmap);
	if (mychannelmap) {
		// Channelmapping provided: Sanity check it.
		if (m<1 || m>2 || p!=1 || (n!=((mynrchannels[0] > mynrchannels[1]) ? mynrchannels[0] : mynrchannels[1]))) {
			PsychErrorExitMsg(PsychError_user, "Invalid size of 'selectchannels' matrix argument: Must be a one- or two row by max(channels) column matrix!");
		}
		
		// Basic check ok. Build ASIO host specific mapping structure:
		#if PSYCH_SYSTEM == PSYCH_WINDOWS
			// Check for ASIO: This only works for ASIO host API...
			if (Pa_GetHostApiInfo(referenceDevInfo->hostApi)->type == paASIO) {
				// MS-Windows and connected to an ASIO device. Good. Try to assign channel mapping:
				if (mode & kPortAudioPlayBack) {
					// Playback mappings:
					outputParameters.hostApiSpecificStreamInfo = (PaAsioStreamInfo*) &outhostapisettings;
					outhostapisettings.size = sizeof(PaAsioStreamInfo);
					outhostapisettings.hostApiType = paASIO;
					outhostapisettings.version = 1;
					outhostapisettings.flags = paAsioUseChannelSelectors;
					outhostapisettings.channelSelectors = (int*) &outputmappings[0];
					for (i=0; i<mynrchannels[0]; i++) outputmappings[i] = (int) mychannelmap[i * m];
					if (verbosity > 3) {
						printf("PTB-INFO: Will try to use the following logical channel -> device channel mappings for sound output to audio stream %i :\n", audiodevicecount); 
						for (i=0; i<mynrchannels[0]; i++) printf("%i --> %i : ", i+1, outputmappings[i]);
						printf("\n\n");
					}
				}
				
				if (mode & kPortAudioCapture) {
					// Capture mappings:
					inputParameters.hostApiSpecificStreamInfo = (PaAsioStreamInfo*) &inhostapisettings;
					inhostapisettings.size = sizeof(PaAsioStreamInfo);
					inhostapisettings.hostApiType = paASIO;
					inhostapisettings.version = 1;
					inhostapisettings.flags = paAsioUseChannelSelectors;
					inhostapisettings.channelSelectors = (int*) &inputmappings[0];
					// Index into first row of one-row matrix or 2nd row of two-row matrix:
					for (i=0; i<mynrchannels[1]; i++) inputmappings[i] = (int) mychannelmap[(i * m) + (m-1)];
					if (verbosity > 3) {
						printf("PTB-INFO: Will try to use the following logical channel -> device channel mappings for sound capture from audio stream %i :\n", audiodevicecount); 
						for (i=0; i<mynrchannels[1]; i++) printf("%i --> %i : ", i+1, inputmappings[i]);
						printf("\n\n");
					}
				}
				// Mappings setup up. The PortAudio library will sanity check this further against device constraints...
			}
			else {
				// Non ASIO device: No ASIO support --> No channel mapping support.
				if (verbosity > 2) printf("PTB-WARNING: Provided 'selectchannels' channel mapping is ignored because this is not an ASIO enabled sound device.\n");
			}
		
		#else
			// Non MS-Windows system: No ASIO support --> No channel mapping support.
			if (verbosity > 2) printf("PTB-WARNING: Provided 'selectchannels' channel mapping is ignored because this is not MS-Windows running on an ASIO enabled sound device.\n");
		#endif
	}
	
	// Set channel count:
	outputParameters.channelCount = mynrchannels[0];	// Number of output channels.
	inputParameters.channelCount = mynrchannels[1];		// Number of input channels.
	
	// Fix sample format to float for now...
	outputParameters.sampleFormat = paFloat32;
	inputParameters.sampleFormat  = paFloat32;

	// Setup buffersize:
	if (buffersize == 0) {
		// No specific buffersize requested:
		if (latencyclass < 3) {
			// At levels < 3, the frames per buffer is derived from
			// the requested latency in seconds. Portaudio will never
			// choose a value smaller than 64 frames per buffer or smaller
			// than what the device suggests as minimum safe value.
			buffersize = paFramesPerBufferUnspecified;
		}
		else {
			if (Pa_GetHostApiInfo(referenceDevInfo->hostApi)->type == paCoreAudio) {
				buffersize = 64; // Lowest setting that is safe on a fast MacBook-Pro.
			}
			else {
				// ASIO/ALSA/others: Leave it unspecified to get optimal selection by lower level driver:
				// Esp. on Windows/ASIO and with Linux/ALSA, basically any choice other than paFramesPerBufferUnspecified
				// will just lead to trouble and less optimal results, as the sound subsystems are very good at
				// choosing this parameter optimally if allowed, but have a hard job to cope with any user-enforced choices.
				buffersize = paFramesPerBufferUnspecified;
			}
		}
	}
	
	// Now we have auto-selected buffersize or user provided override...
	
	// Setup samplerate:
	if (freq == 0) {
		// No specific frequency requested:
		if (latencyclass < 3) {
			// At levels < 3, we select the device specific default.
			freq = referenceDevInfo->defaultSampleRate;
		}
		else {
			freq = 96000; // Go really high...
		}
	}
	
	// Now we have auto-selected frequency or user provided override...

	// Set requested latency: In class 0 we choose device recommendation for dropout-free operation, in
	// all higher (lowlat) classes we request zero latency. PortAudio will
	// clamp this request to something safe internally.
	switch (Pa_GetHostApiInfo(referenceDevInfo->hostApi)->type) {
		case paCoreAudio:	// CoreAudio driver will automatically clamp to safe minimum. Around 0.7 msecs.
		case paWDMKS:		// dto. for Windows kernel streaming.
			lowlatency = 0.0;
		break;
		
		case paMME:		// No such a thing as low latency, but easy to kill the machine with too low settings!
			lowlatency = 0.1; // Play safe, request 100 msecs. Otherwise terrible things may happen!
		break;
		
		case paDirectSound:	// DirectSound defaults to 120 msecs, which is way too much! It doesn't accept 0.0 msecs.
			lowlatency = 0.02;	// Choose some half-way safe tradeoff: 20 msecs.
		break;
		
		case paASIO:		
			// ASIO: A value of zero would set safe (and high latency!) defaults. Too small values get
			// clamped to a safe minimum by the driver, so we select a very small positive value, say
			// 1 msec to get lowest possible latency for latencyclass of at least 2. In latency class 1
			// we play a bit safer and go for 5 msecs:
			lowlatency = (latencyclass >= 2) ? 0.001 : 0.005;
		break;

		case paALSA:	
			// For ALSA we choose 10 msecs by default, lowering to 5 msecs if exp. requested. Experience
			// shows that the effective latency will be only a fraction of this, so we are good.
			// Otoh going too low could cause dropouts on the tested MacbookPro 2nd generation...
			// This will need more lab testing and tweaking - and the user can override anyway...
			lowlatency = (latencyclass > 2) ? 0.005 : 0.010;
		break;

		default:			// Not the safest assumption for non-verified Api's, but we'll see...
			lowlatency = 0.0;
	}
	
	if (suggestedLatency == -1.0) {
		// None provided: Choose default based on latency mode:
		outputParameters.suggestedLatency = (latencyclass == 0 && outputDevInfo) ? outputDevInfo->defaultHighOutputLatency : lowlatency;
		inputParameters.suggestedLatency  = (latencyclass == 0 && inputDevInfo) ? inputDevInfo->defaultHighInputLatency : lowlatency;
	}
	else {
		// Override provided: Use it.
		outputParameters.suggestedLatency = suggestedLatency;
		inputParameters.suggestedLatency  = suggestedLatency;
	}

	#if PSYCH_SYSTEM == PSYCH_OSX
	// Apply OS/X CoreAudio specific optimizations:
	if (latencyclass > 1) {
		unsigned long flags = paMacCore_ChangeDeviceParameters;
		if (latencyclass > 3) flags|= paMacCore_FailIfConversionRequired;
		paSetupMacCoreStreamInfo( &hostapisettings, flags);
		outputParameters.hostApiSpecificStreamInfo = (paMacCoreStreamInfo*) &hostapisettings;
		inputParameters.hostApiSpecificStreamInfo = (paMacCoreStreamInfo*) &hostapisettings;
	}
	#endif
	
	// Our stream shall be primed initially with our callbacks data, not just with zeroes.
	// In high latency-mode 0, we request sample clipping and dithering, so sound is more
	// high quality on Windows. In low-latency mode, we safe the computation time for that.
	sflags = paPrimeOutputBuffersUsingStreamCallback;
	sflags = (latencyclass <= 0) ? sflags : (sflags | paClipOff | paDitherOff);

	// Try to create & open stream:
	err = Pa_OpenStream(
            &stream,															/* Return stream pointer here on success. */
            ((mode & kPortAudioCapture) ?  &inputParameters : NULL),			/* Requested input settings, or NULL in pure playback case. */
			((mode & kPortAudioPlayBack) ? &outputParameters : NULL),			/* Requested input settings, or NULL in pure playback case. */
            freq,																/* Requested sampling rate. */
            buffersize,															/* Requested buffer size. */
            sflags,																/* Define special stream property flags. */
            paCallback,															/* Our processing callback. */
            &audiodevices[audiodevicecount]);									/* Our own device info structure */
            
	if(err!=paNoError || stream == NULL) {
			printf("PTB-ERROR: Failed to open audio device %i. PortAudio reports this error: %s \n", audiodevicecount, Pa_GetErrorText(err));
			PsychErrorExitMsg(PsychError_system, "Failed to open PortAudio audio device.");
	}
	
	// Setup our final device structure:
	audiodevices[audiodevicecount].opmode = mode;
	audiodevices[audiodevicecount].stream = stream;
	audiodevices[audiodevicecount].streaminfo = Pa_GetStreamInfo(stream);
	audiodevices[audiodevicecount].hostAPI = Pa_GetHostApiInfo(referenceDevInfo->hostApi)->type;
	audiodevices[audiodevicecount].startTime = 0.0;
	audiodevices[audiodevicecount].state = 0;
	audiodevices[audiodevicecount].repeatCount = 1;
	audiodevices[audiodevicecount].outputbuffer = NULL;
	audiodevices[audiodevicecount].outputbuffersize = 0;
	audiodevices[audiodevicecount].inputbuffer = NULL;
	audiodevices[audiodevicecount].inputbuffersize = 0;
	audiodevices[audiodevicecount].outchannels = mynrchannels[0];
	audiodevices[audiodevicecount].inchannels = mynrchannels[1];
	audiodevices[audiodevicecount].latencyBias = 0.0;
	
	#if PSYCH_SYSTEM == PSYCH_OSX
		// Query low-level audio driver of the CoreAudio HAL for hardware latency:
		// Hmm, maybe not. Don't know if it is worth the hazzle... This is a constant that
		// doesn't change throughout the lifetime of a machine, and it only makes up for about
		// 0.3 msecs on a typical setup. Maybe if a user really needs that 0.3 msecs extra
		// precision, he should simply download the free HALLab from Apple's Website or Developer
		// tools, run it once, read the displayed constant and set it manually at PortAudio startup.
		
		// PaMacCoreStream* corestream = (PaMacCoreStream*) audiodevices[audiodevicecount].stream;
		// audiodevices[audiodevicecount].latencyBias = 
	#endif
	
	if (verbosity > 3) {
		printf("PTB-INFO: New audio device with handle %i opened as PortAudio stream:\n",audiodevicecount);

		if (audiodevices[audiodevicecount].opmode & kPortAudioPlayBack) {
			printf("PTB-INFO: For %i channels Playback: Audio subsystem is %s, Audio device name is ", audiodevices[audiodevicecount].outchannels, Pa_GetHostApiInfo(outputDevInfo->hostApi)->name);
			printf("%s\n", outputDevInfo->name);
		}

		if (audiodevices[audiodevicecount].opmode & kPortAudioCapture) {
			printf("PTB-INFO: For %i channels Capture: Audio subsystem is %s, Audio device name is ", audiodevices[audiodevicecount].inchannels, Pa_GetHostApiInfo(inputDevInfo->hostApi)->name);
			printf("%s\n", inputDevInfo->name);
		}
		
		printf("PTB-INFO: Real samplerate %f Hz. Input latency %f msecs, Output latency %f msecs.\n",
				audiodevices[audiodevicecount].streaminfo->sampleRate, audiodevices[audiodevicecount].streaminfo->inputLatency * 1000.0,
				audiodevices[audiodevicecount].streaminfo->outputLatency * 1000.0);
	}

	// Return device handle:
	PsychCopyOutDoubleArg(1, kPsychArgOptional, (double) audiodevicecount);
	
	// One more audio device...
	audiodevicecount++;

    return(PsychError_none);	
}

/* PsychPortAudio('Close') - Close an audio device via PortAudio.
 */
PsychError PSYCHPORTAUDIOClose(void) 
{
 	static char useString[] = "PsychPortAudio('Close' [, pahandle]);";
	static char synopsisString[] = 
		"Close a PortAudio audio device. The optional 'pahandle' is the handle of the device to close. If pahandle "
		"is omitted, all audio devices will be closed and the driver will shut down.\n";
	static char seeAlsoString[] = "Open GetDeviceSettings ";	 
  	
	int pahandle= -1;
	
	// Setup online help: 
	PsychPushHelp(useString, synopsisString, seeAlsoString);
	if(PsychIsGiveHelp()) {PsychGiveHelp(); return(PsychError_none); };
	
	PsychErrorExit(PsychCapNumInputArgs(1));     // The maximum number of inputs
	PsychErrorExit(PsychRequireNumInputArgs(0)); // The required number of inputs	
	PsychErrorExit(PsychCapNumOutputArgs(0));	 // The maximum number of outputs

	// Make sure PortAudio is online:
	PsychPortAudioInitialize();

	PsychCopyInIntegerArg(1, kPsychArgOptional, &pahandle);
	if (pahandle == -1) {
		// Full shutdown requested:
		PsychPortAudioExit();
	}
	else {
		// Close one device:
		if (pahandle < 0 || pahandle>=MAX_PSYCH_AUDIO_DEVS || audiodevices[pahandle].stream == NULL) PsychErrorExitMsg(PsychError_user, "Invalid audio device handle provided.");
		PsychPACloseStream(pahandle);
		
		// All devices down? Shutdown PortAudio if so:
		if (audiodevicecount == 0) PsychPortAudioExit();
	}
	
	return(PsychError_none);
}

/* PsychPortAudio('FillBuffer') - Fill audio outputbuffer of a device with data.
 */
PsychError PSYCHPORTAUDIOFillAudioBuffer(void) 
{
 	static char useString[] = "underflow = PsychPortAudio('FillBuffer', pahandle, bufferdata [, streamingrefill=0);";
	static char synopsisString[] = 
		"Fill audio data playback buffer of a PortAudio audio device. 'pahandle' is the handle of the device "
		"whose buffer is to be filled. 'bufferdata' is a Matlab double matrix with audio data in double format. Each "
		"row of the matrix specifies one sound channel, each column one sample for each channel. Only floating point "
		"values in double precision are supported. Samples need to be in range -1.0 to +1.0, 0.0 for silence. This is "
		"intentionally a very restricted interface. For lowest latency and best timing we want you to provide audio "
		"data exactly at the optimal format and sample rate, so the driver can safe computation time and latency for "
		"expensive sample rate conversion, sample format conversion, and bounds checking/clipping.\n"
		"'streamingrefill' optional: If set to 1, ask the driver to refill the buffer immediately while playback "
		"is active. You can think of this as appending the audio data to the audio data already present in the buffer. "
		"This is useful for streaming playback or for creating live audio feedback loops. However, the current implementation "
		"doesn't really append the audio data. Instead it replaces already played audio data with your new data. This means "
		"that if you try to refill more than what has been actually played, this function will wait until enough storage space "
		"is available. It will also fail if you try to refill more than the total buffer capacity. Default is to not do "
		"streaming refills, i.e., the bufferis filled in one batch while playback is stopped.";
		
	static char seeAlsoString[] = "Open GetDeviceSettings ";	 
  	
	int inchannels, insamples, p, buffersize;
	double*	indata = NULL;
	float*  outdata = NULL;
	int pahandle   = -1;
	int streamingrefill = 0;
	int underrun = 0;
	
	// Setup online help: 
	PsychPushHelp(useString, synopsisString, seeAlsoString);
	if(PsychIsGiveHelp()) {PsychGiveHelp(); return(PsychError_none); };
	
	PsychErrorExit(PsychCapNumInputArgs(3));     // The maximum number of inputs
	PsychErrorExit(PsychRequireNumInputArgs(2)); // The required number of inputs	
	PsychErrorExit(PsychCapNumOutputArgs(1));	 // The maximum number of outputs

	// Make sure PortAudio is online:
	PsychPortAudioInitialize();

	PsychCopyInIntegerArg(1, kPsychArgRequired, &pahandle);
	if (pahandle < 0 || pahandle>=MAX_PSYCH_AUDIO_DEVS || audiodevices[pahandle].stream == NULL) PsychErrorExitMsg(PsychError_user, "Invalid audio device handle provided.");
	if (audiodevices[pahandle].opmode & kPortAudioPlayBack == 0) PsychErrorExitMsg(PsychError_user, "Audio device has not been opened for audio playback, so this call doesn't make sense.");

	PsychAllocInDoubleMatArg(2, kPsychArgRequired, &inchannels, &insamples, &p, &indata);
	if (inchannels != audiodevices[pahandle].outchannels) {
		printf("PTB-ERROR: Audio device %i has %i output channels, but provided matrix has non-matching number of %i rows.\n", pahandle, audiodevices[pahandle].outchannels, inchannels);
		PsychErrorExitMsg(PsychError_user, "Number of rows of audio data matrix doesn't match number of output channels of selected audio device.\n");
	}
	
	if (insamples < 1) PsychErrorExitMsg(PsychError_user, "You must provide at least 1 sample in your audio buffer!");
	if (p!=1) PsychErrorExitMsg(PsychError_user, "Audio data matrix must be a 2D matrix, but this one is not a 2D matrix!");
	
	// Get optional streaming refill flag:
	PsychCopyInIntegerArg(3, kPsychArgOptional, &streamingrefill);
	
	// Full refill or streaming refill?
	if (streamingrefill <= 0) {
		// Standard refill with possible buffer reallocation. Engine needs to be
		// stopped, full reset of engine at refill:
		// Wait for playback on this stream to finish, before refilling it:
		while (audiodevices[pahandle].state > 0) {
			// Sleep a millisecond:
			PsychWaitIntervalSeconds(0.001);
		}
		
		// Ok, everything sane, fill the buffer:
		buffersize = sizeof(float) * inchannels * insamples;
		if (audiodevices[pahandle].outputbuffer && (audiodevices[pahandle].outputbuffersize != buffersize)) {
			free(audiodevices[pahandle].outputbuffer);
			audiodevices[pahandle].outputbuffer = NULL;
			audiodevices[pahandle].outputbuffersize = 0;
		}
		
		if (audiodevices[pahandle].outputbuffer == NULL) {
			audiodevices[pahandle].outputbuffersize = buffersize;
			audiodevices[pahandle].outputbuffer = (float*) malloc(buffersize);
			if (audiodevices[pahandle].outputbuffer==NULL) PsychErrorExitMsg(PsychError_outofMemory, "Out of system memory when trying to allocate audio buffer.");
		}
		
		// Reset play position:
		audiodevices[pahandle].playposition = 0;
		
		// Copy the data, convert it from double to float:
		outdata = audiodevices[pahandle].outputbuffer;
		while(buffersize) {
			*(outdata++) = (float) *(indata++);
			buffersize-=sizeof(float);
		}
				
		// Reset write position to end of buffer:
		audiodevices[pahandle].writeposition = inchannels * insamples;
	}
	else {
		// Streaming refill while playback is running:

		// Engine stopped?
		if (audiodevices[pahandle].state == 0) PsychErrorExitMsg(PsychError_user, "Audiodevice not in playback mode! Can't do a streaming buffer refill while stopped.");

		// No buffer allocated?
		if (audiodevices[pahandle].outputbuffer == NULL) PsychErrorExitMsg(PsychError_user, "No audio buffer allocated! You must call this method once before start of playback to initially allocate a buffer of sufficient size.");

		// Buffer of sufficient size for a streaming refill of this amount?
		buffersize = sizeof(float) * inchannels * insamples;
		if (audiodevices[pahandle].outputbuffersize < buffersize) PsychErrorExitMsg(PsychError_user, "Total capacity of audio buffer is too small for a refill of this size! Allocate an initial buffer of at least the size of the biggest refill.");

		// Check for buffer underrun:
		if (audiodevices[pahandle].writeposition < audiodevices[pahandle].playposition) underrun = 1;

		// Boundary conditions met. Can we refill immediately or do we need to wait for playback
		// position to progress far enough?
		while (((audiodevices[pahandle].outputbuffersize / sizeof(float)) - (audiodevices[pahandle].writeposition - audiodevices[pahandle].playposition) - inchannels) <= (inchannels * insamples)) {
			// Sleep a millisecond:
			PsychWaitIntervalSeconds(0.001);
		} 
		
		// Ok enough headroom for batch streaming refill:
		
		// Copy the data, convert it from double to float, take ringbuffer wraparound into account:
		while(buffersize > 0) {
			// Fetch next sample and copy it to matrix:
			audiodevices[pahandle].outputbuffer[(audiodevices[pahandle].writeposition % (audiodevices[pahandle].outputbuffersize / sizeof(float)))] = (float) *(indata++);
			
			// Update sample write counter:
			audiodevices[pahandle].writeposition++;
			
			// Decrement copy counter:
			buffersize-=sizeof(float);
		}
		
		// Check for buffer underrun:
		if (audiodevices[pahandle].writeposition < audiodevices[pahandle].playposition) underrun = 1;

		if ((underrun > 0) && (verbosity > 1)) printf("PsychPortAudio-WARNING: Underrun of audio playback buffer detected during streaming refill. Some sound data will be skipped!\n");
	}

	// Copy out underrun flag:
	PsychCopyOutDoubleArg(1, FALSE, (double) underrun);

	// Buffer ready.
	return(PsychError_none);
}

/* PsychPortAudio('GetAudioData') - Retrieve captured audio data.
 */
PsychError PSYCHPORTAUDIOGetAudioData(void) 
{
 	static char useString[] = "[audiodata absrecposition overflow cstarttime] = PsychPortAudio('GetAudioData', pahandle [, amountToAllocateSecs][, minimumAmountToReturnSecs][, maximumAmountToReturnSecs]);";
	static char synopsisString[] = 
		"Retrieve captured audio data from a audio device. 'pahandle' is the handle of the device "
		"whose data is to be retrieved. 'audiodata' is a Matlab double matrix with audio data in double format. Each "
		"row of the matrix returns one sound channel, each column one sample for each channel. Only floating point "
		"values in double precision are returned. Samples will be in range -1.0 to +1.0, 0.0 for silence. This is "
		"intentionally a very restricted interface. For lowest latency and best timing we want you to accept audio "
		"data exactly at the optimal format and sample rate, so the driver can safe computation time and latency for "
		"expensive sample rate conversion, sample format conversion, and bounds checking/clipping.\n"
		"You must call this function once before start of capture operations to allocate an internal buffer "
		"that stores captured audio data inbetween your periodic calls. Provide 'amountToAllocateSecs' as "
		"requested buffersize in seconds. After start of capture you must call this function periodically "
		"at least every 'amountToAllocateSecs' seconds to drain the internal buffer into your Matlab/Octave "
		"matrix 'audiodata'. If you fail to call the function frequently enough, sound data will get lost!\n"
		"'minimumAmountToReturnSecs' optional minimum amount of recorded data to return at each call. The "
		"driver will only return control to your script when it was able to collect at least that amount "
		"of seconds of sound data - or if the capture engine was stopped. If you don't set this parameter, "
		"the driver will return immediately, giving you whatever amount of sound data was available - including "
		"an empty matrix if nothing was available.\n"
		"'maximumAmountToReturnSecs' allows you to optionally restrict the amount of returned sound data to "
		"a specific duration in seconds. By default, you'll get whatever is available.\n"
		"If you provide both, 'minimumAmountToReturnSecs' and 'maximumAmountToReturnSecs' and set them to equal "
		"values (but significantly lower than the 'amountToAllocateSecs' buffersize!!) then you'll always "
		"get an 'audiodata' matrix back that is of a fixed size. This may be convenient for postprocessing "
		"in Matlab. It may also reduce or avoid Matlab memory fragmentation...\n\n"
		"\nOptional return arguments other than 'audiodata':\n\n"
		"'absrecposition' is the absolute position (in samples) of the first column in the returned data matrix, "
		"assuming that sample zero was the very first recorded sample in this session. The count is reset each time "
		"you start a new capture session via call to PsychPortAudio('Start').\n"
		"Each call to this function will return a new chunk of recorded sound data. The 'absrecposition' provides "
		"you with absolute matrix column indices to stitch together the results of all calls into one seamless "
		"recording if you want. 'overflow' if this flag is zero then everything went fine. If it is one then you "
		"didn't manage to call this function frequent enough, the capacity of the internal recording buffer was "
		"exceeded and therefore you lost captured sound data, i.e., there is a gap in your recording. When "
		"initially allocating the internal buffer, make sure to allocate it big enough so it is able to easily "
		"store all recorded data inbetween your calls to 'GetAudioData'. Example: You expect to call this routine "
		"once every second in your trial loop, then allocate a sound buffer of at least 2 seconds for some security "
		"headroom. If you know that the recording time of each recording has an upper bound then you can allocate "
		"an internal buffer of sufficient size and fetch the buffer all at once at the end of a recording.\n"
		"'ctsstarttime' this is an estimate of the system time (in seconds) when the very first sample of this "
		"recording was captured by the sound input of your hardware. This is only a rough estimate, not to be "
		"trusted down to the millisecond level, at least not without former careful calibration of your setup!\n";

	static char seeAlsoString[] = "Open GetDeviceSettings ";	 
  	
	int inchannels, insamples, p, buffersize, maxSamples;
	double*	indata = NULL;
	float*  outdata = NULL;
	int pahandle   = -1;
	double allocsize;
	double minSecs, maxSecs, minSamples;
	int overrun = 0;
	
	// Setup online help: 
	PsychPushHelp(useString, synopsisString, seeAlsoString);
	if(PsychIsGiveHelp()) {PsychGiveHelp(); return(PsychError_none); };
	
	PsychErrorExit(PsychCapNumInputArgs(4));     // The maximum number of inputs
	PsychErrorExit(PsychRequireNumInputArgs(1)); // The required number of inputs	
	PsychErrorExit(PsychCapNumOutputArgs(4));	 // The maximum number of outputs

	// Make sure PortAudio is online:
	PsychPortAudioInitialize();

	PsychCopyInIntegerArg(1, kPsychArgRequired, &pahandle);
	if (pahandle < 0 || pahandle>=MAX_PSYCH_AUDIO_DEVS || audiodevices[pahandle].stream == NULL) PsychErrorExitMsg(PsychError_user, "Invalid audio device handle provided.");
	if (audiodevices[pahandle].opmode & kPortAudioCapture == 0) PsychErrorExitMsg(PsychError_user, "Audio device has not been opened for audio capture, so this call doesn't make sense.");

	buffersize = audiodevices[pahandle].inputbuffersize;
	
	// Copy in optional amount of buffer memory to allocate for internal recording ringbuffer:
	allocsize = 0;
	PsychCopyInDoubleArg(2, kPsychArgOptional, &allocsize);

	// Internal capture ringbuffer already allocated?
	if (buffersize == 0) {
		// Nope. This call needs to allocate it.
		if (allocsize <= 0) PsychErrorExitMsg(PsychError_user, "You must first call this function with a positive 'amountToAllocateSecs' argument to allocate internal bufferspace first!");
		
		// Ready to allocate ringbuffer memory outside this if-clause...
	}
	else {
		// Buffer already allocated. Need to realloc?
		if (allocsize > 0) {
			// Calling script wants to reallocate the buffer. Check if this is possible here:
			
			// Test 1: Pending samples to read from current ringbuffer?
			if (audiodevices[pahandle].readposition < audiodevices[pahandle].recposition) PsychErrorExitMsg(PsychError_user, "Tried to resize internal buffer without emptying it beforehand. You must drain the buffer before resizing it!");

			// Test 2: Engine running?
			if (audiodevices[pahandle].state > 0) PsychErrorExitMsg(PsychError_user, "Tried to resize internal buffer while recording engine is running! You must stop recording before resizing the buffer!");
			
			// Ok, reallocation allowed. Delete old buffer:
			audiodevices[pahandle].inputbuffersize = 0;
			free(audiodevices[pahandle].inputbuffer);
			audiodevices[pahandle].inputbuffer = NULL;
			
			// At this point we are ready to re-allocate ringbuffer outside this if-clause...
		}
	}
	
	// Still (re-)allocation wanted?
	if (allocsize > 0) {
		// Calculate needed buffersize in samples: Convert allocsize in seconds to size in bytes:
		audiodevices[pahandle].inputbuffersize = sizeof(float) * ((int) (allocsize * audiodevices[pahandle].streaminfo->sampleRate)) * audiodevices[pahandle].inchannels;
		audiodevices[pahandle].inputbuffer = (float*) calloc(1, audiodevices[pahandle].inputbuffersize);
		if (audiodevices[pahandle].inputbuffer == NULL) PsychErrorExitMsg(PsychError_outofMemory, "Free system memory exhausted when trying to allocate audio recording buffer!");

		// This was an (re-)allocation call, so no data is pending in the buffer.
		// Therefore we don't return any data, just reset the counters:
		audiodevices[pahandle].recposition = 0;
		audiodevices[pahandle].readposition = 0;
		return(PsychError_none);
	}
	
	// This is not an allocation call, but a real data fetch call:

	// Get optional "minimum amount to return" argument:
	// We default to "whatever we can get" ie. zero seconds.
	minSecs = 0;
	PsychCopyInDoubleArg(3, kPsychArgOptional, &minSecs);

	// Get optional "maximum amount to return" argument:
	// We default to "whatever we can get" ie. infinite seconds.
	maxSecs = 0;
	PsychCopyInDoubleArg(4, kPsychArgOptional, &maxSecs);

	// How much samples are available in ringbuffer to fetch?
	insamples = audiodevices[pahandle].recposition - audiodevices[pahandle].readposition;
	
	// Convert amount of available data into seconds and check if our minimum
	// requirements are fulfilled:
	if (minSecs > 0) {
		// Convert seconds to samples:
		minSamples = minSecs * ((double) audiodevices[pahandle].streaminfo->sampleRate) * ((double) audiodevices[pahandle].inchannels) + ((double) audiodevices[pahandle].inchannels);

		// Bigger than buffersize? That would be a no no...
		if ((minSamples * sizeof(float)) > audiodevices[pahandle].inputbuffersize) {
			PsychErrorExitMsg(PsychError_user, "Invalid 'minimumAmountToReturnSecs' parameter: The requested minimum is bigger than the whole capture buffer size!'");			
		}
		
		// Loop until either request is fullfillable or the device gets stopped - in which
		// case we'll never be able to fullfill the request...
		while (((double) insamples < minSamples) && (audiodevices[pahandle].state > 0)) {
			// Compute amount of time to elapse before request could be fullfilled:
			minSecs = (minSamples - (double) insamples) / ((double) audiodevices[pahandle].inchannels) / ((double) audiodevices[pahandle].streaminfo->sampleRate);
			// Ok, required data will be available earliest in 'minSecs' seconds. Sleep until then:
			PsychWaitIntervalSeconds(minSecs);
			// We've slept at least the estimated amount of required time. Recalculate amount
			// of available sound data and check again...
			insamples = audiodevices[pahandle].recposition - audiodevices[pahandle].readposition;
		}
	}
	
	// Never ever fetch the samples for the last sampleframe. We do not want to fetch
	// a possibly not yet updated or incomplete sample frame. Leave this to next call
	// of this function. Well, unless state is zero == engine stopped. In that case we
	// know that the playhead won't move anymore and we can safely fetch all remaining
	// data.
	if (audiodevices[pahandle].state > 0) {
		insamples = insamples - (insamples % audiodevices[pahandle].inchannels);
		insamples-= audiodevices[pahandle].inchannels;
	}
	
	insamples = (insamples < 0) ? 0 : insamples;
	buffersize = insamples * sizeof(float);

	// Buffer "overflow" detected?
	if (buffersize > audiodevices[pahandle].inputbuffersize) {
		// Ok, the buffer did overrun and captured data was lost. Limit returned data
		// to buffersize and set the overrun flag, optionally output a warning:
		buffersize = audiodevices[pahandle].inputbuffersize;
		insamples = buffersize / sizeof(float);
		
		// Set overrun flag:
		overrun = 1;
		
		if (verbosity > 1) printf("PsychPortAudio-WARNING: Overflow of audio capture buffer detected. Some sound data will be lost!\n");
	}
	
	// Limitation of returned amount of data wanted?
	if (maxSecs > 0) {
		// Yes. Convert maximum amount in seconds to maximum amount in samples:
		maxSamples = (int) (ceil(maxSecs * ((double) audiodevices[pahandle].streaminfo->sampleRate)) * ((double) audiodevices[pahandle].inchannels));
		// Clamp insamples to that value, if neccessary:
		if (insamples > maxSamples) {
			insamples = maxSamples;
			buffersize = insamples * sizeof(float);
		}
	}
	
	// Allocate output double matrix with matching number of channels and samples:
	PsychAllocOutDoubleMatArg(1, FALSE, audiodevices[pahandle].inchannels, insamples / audiodevices[pahandle].inchannels, 1, &indata);

	// Copy out absolute sample read position of first sample in buffer:
	PsychCopyOutDoubleArg(2, FALSE, (double) (audiodevices[pahandle].readposition / audiodevices[pahandle].inchannels));

	// Copy the data, convert it from float to double: Take ringbuffer wraparound into account:
	while(buffersize > 0) {
		// Fetch next sample and copy it to matrix:
		*(indata++) = (double) audiodevices[pahandle].inputbuffer[(audiodevices[pahandle].readposition % (audiodevices[pahandle].inputbuffersize / sizeof(float)))];

		// Update sample read counter:
		audiodevices[pahandle].readposition++;
		
		// Decrement copy counter:
		buffersize-=sizeof(float);
	}

	// Copy out overrun flag:
	PsychCopyOutDoubleArg(3, FALSE, (double) overrun);

	// Return capture timestamp in system time of first captured sample in this session. This is a bit problematic:
	// In full-duplex mode, at least OS/X doesn't return separate timestamps, so we'll provide the playback onset time
	// instead - the best we can do. In pure capture mode we get a capture timestamp...
	PsychCopyOutDoubleArg(4, FALSE, (audiodevices[pahandle].captureStartTime > 0) ? audiodevices[pahandle].captureStartTime : audiodevices[pahandle].startTime);
	
	// Buffer ready.
	return(PsychError_none);
}

/* PsychPortAudio('RescheduleStart') - Set new start time for an already running audio device via PortAudio.
 */
PsychError PSYCHPORTAUDIORescheduleStart(void) 
{
 	static char useString[] = "startTime = PsychPortAudio('RescheduleStart', pahandle, when [, waitForStart=0]);";
	static char synopsisString[] = 
		"Modify requested start time 'when' of an already started PortAudio audio device.\n"
		"After you've started an audio device via the 'Start' subfunction, but *before* the "
		"device has really started playback (because the 'when' time provided to the 'Start' "
		"method is still far in the future), you can use this function to reschedule the start for "
		"a different 'when' time - including a value of zero for immediate start.\n"
		"\n"
		"The 'pahandle' is the handle of the device to start. Starting a "
		"device means: Start playback of output devices, start recording on capture device, do both on "
		"full duplex devices. 'waitForStart' if set to 1 will wait until device has really started, default "
		"is to continue immediately, ie. only schedule start of device. 'when' Requested time, when device "
		"should start. Defaults to zero, i.e. start immediately. If set to a non-zero system time, PTB will "
		"do its best to start the device at the requested time, but the accuracy of start depends on the "
		"operating system, audio hardware and system load. If 'waitForStart' is set to non-zero value, ie "
		"if PTB should wait for sound onset, then the optional return argument 'startTime' will contain an "
		"estimate of when the first audio sample hit the speakers, i.e., the real start time.\n"
		"Please note that the 'when' value always refers to playback, so it defines the starttime of "
		"playback. The start time of capture is related to the start time of playback in duplex mode, "
		"but it isn't the same. In pure capture mode (without playback), 'when' will be ignored and "
		"capture always starts immediately. See the help for subfunction 'GetStatus' for more info on "
		"the meaning of the different timestamps. ";
		
	static char seeAlsoString[] = "Open";	 
  	
	PaError err;
	int pahandle= -1;
	int waitForStart = 0;
	double when = 0.0;
	
	// Setup online help: 
	PsychPushHelp(useString, synopsisString, seeAlsoString);
	if(PsychIsGiveHelp()) {PsychGiveHelp(); return(PsychError_none); };
	
	PsychErrorExit(PsychCapNumInputArgs(3));     // The maximum number of inputs
	PsychErrorExit(PsychRequireNumInputArgs(2)); // The required number of inputs	
	PsychErrorExit(PsychCapNumOutputArgs(1));	 // The maximum number of outputs

	// Make sure PortAudio is online:
	PsychPortAudioInitialize();

	PsychCopyInIntegerArg(1, kPsychArgRequired, &pahandle);
	if (pahandle < 0 || pahandle>=MAX_PSYCH_AUDIO_DEVS || audiodevices[pahandle].stream == NULL) PsychErrorExitMsg(PsychError_user, "Invalid audio device handle provided.");
	if (audiodevices[pahandle].state != 1) PsychErrorExitMsg(PsychError_user, "Device already active, or even finished!");

	PsychCopyInDoubleArg(2, kPsychArgRequired, &when);
	if (when < 0) PsychErrorExitMsg(PsychError_user, "Invalid setting for 'when'. Valid values are zero or greater.");
	
	// Setup rescheduled target start time:
	audiodevices[pahandle].startTime = when;

	PsychCopyInIntegerArg(3, kPsychArgOptional, &waitForStart);
	if (waitForStart < 0) PsychErrorExitMsg(PsychError_user, "Invalid setting for 'waitForStart'. Valid values are zero or greater.");

	if (waitForStart>0) {
		// Wait for real start of device:
		while(audiodevices[pahandle].state == 1) {
			PsychWaitIntervalSeconds(0.001);
		}
		
		// Ok, relevant audio buffer with real sound onset submitted to engine.
		// We now have an estimate of real sound onset in startTime, wait until
		// then:
		PsychWaitUntilSeconds(audiodevices[pahandle].startTime);
		
		// Engine should run now. Return real onset time:
		PsychCopyOutDoubleArg(1, kPsychArgOptional, audiodevices[pahandle].startTime);
	}
	else {
		// Return empty zero timestamp to signal that this info is not available:
		PsychCopyOutDoubleArg(1, kPsychArgOptional, 0.0);
	}
	
	return(PsychError_none);
}


/* PsychPortAudio('Start') - Start an audio device via PortAudio.
 */
PsychError PSYCHPORTAUDIOStartAudioDevice(void) 
{
 	static char useString[] = "startTime = PsychPortAudio('Start', pahandle [, repetitions=1] [, when=0] [, waitForStart=0] );";
	static char synopsisString[] = 
		"Start a PortAudio audio device. The 'pahandle' is the handle of the device to start. Starting a "
		"device means: Start playback of output devices, start recording on capture device, do both on "
		"full duplex devices. 'waitForStart' if set to 1 will wait until device has really started, default "
		"is to continue immediately, ie. only schedule start of device. 'when' Requested time, when device "
		"should start. Defaults to zero, i.e. start immediately. If set to a non-zero system time, PTB will "
		"do its best to start the device at the requested time, but the accuracy of start depends on the "
		"operating system, audio hardware and system load. If 'waitForStart' is set to non-zero value, ie "
		"if PTB should wait for sound onset, then the optional return argument 'startTime' will contain an "
		"estimate of when the first audio sample hit the speakers, i.e., the real start time.\n"
		"Please note that the 'when' value always refers to playback, so it defines the starttime of "
		"playback. The start time of capture is related to the start time of playback in duplex mode, "
		"but it isn't the same. In pure capture mode (without playback), 'when' will be ignored and "
		"capture always starts immediately. See the help for subfunction 'GetStatus' for more info on "
		"the meaning of the different timestamps. ";
		
	static char seeAlsoString[] = "Open";	 
  	
	PaError err;
	int pahandle= -1;
	int waitForStart = 0;
	int repetitions = 1;
	double when = 0.0;
	
	// Setup online help: 
	PsychPushHelp(useString, synopsisString, seeAlsoString);
	if(PsychIsGiveHelp()) {PsychGiveHelp(); return(PsychError_none); };
	
	PsychErrorExit(PsychCapNumInputArgs(4));     // The maximum number of inputs
	PsychErrorExit(PsychRequireNumInputArgs(1)); // The required number of inputs	
	PsychErrorExit(PsychCapNumOutputArgs(1));	 // The maximum number of outputs

	// Make sure PortAudio is online:
	PsychPortAudioInitialize();

	PsychCopyInIntegerArg(1, kPsychArgRequired, &pahandle);
	if (pahandle < 0 || pahandle>=MAX_PSYCH_AUDIO_DEVS || audiodevices[pahandle].stream == NULL) PsychErrorExitMsg(PsychError_user, "Invalid audio device handle provided.");
	if ((audiodevices[pahandle].opmode & kPortAudioMonitoring) == 0) {
		// Not in monitoring mode: We must have in/outbuffers allocated:
		if ((audiodevices[pahandle].opmode & kPortAudioPlayBack) && (audiodevices[pahandle].outputbuffer == NULL)) PsychErrorExitMsg(PsychError_user, "Sound outputbuffer doesn't contain any sound to play?!?");
		if ((audiodevices[pahandle].opmode & kPortAudioCapture) && (audiodevices[pahandle].inputbuffer == NULL)) PsychErrorExitMsg(PsychError_user, "Sound inputbuffer not prepared/allocated for capture?!?");
	}
	if (audiodevices[pahandle].state > 0) PsychErrorExitMsg(PsychError_user, "Device already started.");

	PsychCopyInIntegerArg(2, kPsychArgOptional, &repetitions);
	if (repetitions < 0) PsychErrorExitMsg(PsychError_user, "Invalid setting for 'repetitions'. Valid values are zero or greater.");

	// Set number of requested repetitions: 0 means loop forever, default is 1 time.
	audiodevices[pahandle].repeatCount = (repetitions == 0) ? -1 : repetitions;

	PsychCopyInDoubleArg(3, kPsychArgOptional, &when);
	if (when < 0) PsychErrorExitMsg(PsychError_user, "Invalid setting for 'when'. Valid values are zero or greater.");
	
	// Setup target start time:
	audiodevices[pahandle].startTime = when;

	PsychCopyInIntegerArg(4, kPsychArgOptional, &waitForStart);
	if (waitForStart < 0) PsychErrorExitMsg(PsychError_user, "Invalid setting for 'waitForStart'. Valid values are zero or greater.");
	
	// Reset statistics values:
	audiodevices[pahandle].batchsize = 0;	
	audiodevices[pahandle].xruns = 0;	
	audiodevices[pahandle].paCalls = 0;
	audiodevices[pahandle].noTime = 0;
	audiodevices[pahandle].captureStartTime = 0;
	
	// Reset recorded samples counter:
	audiodevices[pahandle].recposition = 0;

	// Reset read samples counter: This will discard possibly not yet fetched data.
	audiodevices[pahandle].readposition = 0;

	// Reset play position:
	audiodevices[pahandle].playposition = 0;
	
	// Mark state as hot-started:
	audiodevices[pahandle].state = 1;

	// Try to start stream:
	if ((err=Pa_StartStream(audiodevices[pahandle].stream))!=paNoError) {
		printf("PTB-ERROR: Failed to start audio device %i. PortAudio reports this error: %s \n", pahandle, Pa_GetErrorText(err));
		PsychErrorExitMsg(PsychError_system, "Failed to start PortAudio audio device.");
	}
		
	if (waitForStart>0) {
		// Wait for real start of device:
		while(audiodevices[pahandle].state != 2) {
			PsychWaitIntervalSeconds(0.001);
		}
		
		// Ok, relevant audio buffer with real sound onset submit to engine.
		// We now have an estimate of real sound onset in startTime, wait until
		// then:
		PsychWaitUntilSeconds(audiodevices[pahandle].startTime);
		
		// Engine should run now. Return real onset time:
		PsychCopyOutDoubleArg(1, kPsychArgOptional, audiodevices[pahandle].startTime);
	}
	else {
		// Return empty zero timestamp to signal that this info is not available:
		PsychCopyOutDoubleArg(1, kPsychArgOptional, 0.0);
	}

	return(PsychError_none);
}

/* PsychPortAudio('Stop') - Stop an audio device via PortAudio.
 */
PsychError PSYCHPORTAUDIOStopAudioDevice(void) 
{
 	static char useString[] = "[startTime endPositionSecs xruns] = PsychPortAudio('Stop', pahandle [,waitForEndOfPlayback=0]);";
	static char synopsisString[] = 
		"Stop a PortAudio audio device. The 'pahandle' is the handle of the device to stop.\n"
		"'waitForEndOfPlayback' - If set to 1, this method will wait until playback of the "
		"audio stream finishes by itself. This only makes sense if you perform playback with "
		"a limited number of repetitions. The flag will be ignored when infinite repetition is "
		"requested (as playback would never stop by itself, resulting in a hang) or if this is "
		"a pure recording session.\n"
		"A setting of 0 (which is the default) requests stop of playback without waiting for all "
		"the sound to be finished playing. Sound may continue to be played back for multiple "
		"milliseconds after this call, as this is a polite request to the hardware to finish up.\n"
		"A setting of 2 requests abortion of playback and/or capture as soon as possible with your "
		"sound hardware, even if this creates audible artifacts etc. Abortion may or may not be faster "
		"than a normal stop, this depends on your specific hardware, but our driver tries as hard as "
		"possible to get the hardware to shut up. In a worst-case setting, the hardware would continue "
		"to playback the sound data that is stored in its internal buffers, so the latency for stopping "
		"sound would be the the same as the latency for starting sound as quickly as possible. E.g., "
		"a 2nd generation Intel MacBook Pro seems to have a stop-delay of roughly 5-6 msecs under "
		"optimal conditions (buffersize = 64, frequency=96000, OS/X 10.4.10, no other sound apps running).\n"
		"The optional return argument 'startTime' returns an estimate of when the stopped "
		"stream actually started its playback and/or recording. Its the same timestamp as the one "
		"returned by the start command when executed in waiting mode. 'endPositionSecs' is the final "
		"playback/recording position in seconds. 'xruns' is the number of buffer over- or underruns. "
		"This should be zero if the playback operation was glitch-free, however a zero value doesn't "
		"imply glitch free operation, as the glitch detection algorithm can miss some types of glitches. ";
	
	static char seeAlsoString[] = "Open GetDeviceSettings ";	 
	
	PaError err;
	int pahandle= -1;
	int waitforend = 0;
	
	// Setup online help: 
	PsychPushHelp(useString, synopsisString, seeAlsoString);
	if(PsychIsGiveHelp()) {PsychGiveHelp(); return(PsychError_none); };
	
	PsychErrorExit(PsychCapNumInputArgs(2));     // The maximum number of inputs
	PsychErrorExit(PsychRequireNumInputArgs(1)); // The required number of inputs	
	PsychErrorExit(PsychCapNumOutputArgs(3));	 // The maximum number of outputs

	// Make sure PortAudio is online:
	PsychPortAudioInitialize();

	PsychCopyInIntegerArg(1, kPsychArgRequired, &pahandle);
	if (pahandle < 0 || pahandle>=MAX_PSYCH_AUDIO_DEVS || audiodevices[pahandle].stream == NULL) PsychErrorExitMsg(PsychError_user, "Invalid audio device handle provided.");

	// Get optional wait-flag:
	PsychCopyInIntegerArg(2, kPsychArgOptional, &waitforend);
	
	// Wait for automatic stop of playback if requested: This only makes sense if we
	// are in playback mode and not in infinite playback mode! Would not make sense in
	// looping mode (infinite repetitions of playback) or pure recording mode:
	if ((waitforend == 1) && (audiodevices[pahandle].state > 0) && (audiodevices[pahandle].opmode & kPortAudioPlayBack) && (audiodevices[pahandle].repeatCount != -1)) {
		// Wait until stream finishes by itself:
		while (Pa_IsStreamActive(audiodevices[pahandle].stream)) PsychWaitIntervalSeconds(0.001);
	}
	
	// Soft stop requested (as opposed to fast stop)?
	if (waitforend!=2) {
		// Softstop: Try to stop stream. Skip if already stopped/not yet started:
		if ((audiodevices[pahandle].state > 0) && ((err=Pa_StopStream(audiodevices[pahandle].stream))!=paNoError)) {
			printf("PTB-ERROR: Failed to stop audio device %i. PortAudio reports this error: %s \n", pahandle, Pa_GetErrorText(err));
			PsychErrorExitMsg(PsychError_system, "Failed to stop PortAudio audio device.");
		}
	}
	else {
		// Faststop: Try to abort stream. Skip if already stopped/not yet started:

		// Stream active?
		if (audiodevices[pahandle].state > 0) {
			// Yes. Set the 'state' flag to signal our IO-Thread not to push any audio
			// data anymore, but only zeros for silence.
			audiodevices[pahandle].state = 3;
			
			// Now send abort request to hardware:
			if ((err=Pa_AbortStream(audiodevices[pahandle].stream))!=paNoError) {
				printf("PTB-ERROR: Failed to abort audio device %i. PortAudio reports this error: %s \n", pahandle, Pa_GetErrorText(err));
				PsychErrorExitMsg(PsychError_system, "Failed to fast stop (abort) PortAudio audio device.");
			}
		}
	}

	// Wait for real stop:
	while (Pa_IsStreamActive(audiodevices[pahandle].stream)) PsychWaitIntervalSeconds(0.001);
	
	// Copy out our estimate of when playback really started for the just stopped stream:
	PsychCopyOutDoubleArg(1, kPsychArgOptional, audiodevices[pahandle].startTime);

	// Copy out final playback position (secs) since start:
	PsychCopyOutDoubleArg(2, kPsychArgOptional, ((double)(audiodevices[pahandle].playposition / audiodevices[pahandle].outchannels)) / (double) audiodevices[pahandle].streaminfo->sampleRate);

	// Copy out number of buffer over-/underflows since start:
	PsychCopyOutDoubleArg(3, kPsychArgOptional, audiodevices[pahandle].xruns);
	
	// Mark state as stopped:
	audiodevices[pahandle].state = 0;
	
	return(PsychError_none);
}

/* PsychPortAudio('GetStatus') - Return current status of stream.
 */
PsychError PSYCHPORTAUDIOGetStatus(void) 
{
 	static char useString[] = "status = PsychPortAudio('GetStatus', pahandle);";
	static char synopsisString[] = 
		"Returns 'status', a struct with status information about the current state of device 'pahandle'.\n"
		"The struct contains the following fields:\n"
		"Active: Can be 1 if playback or recording is active, or 0 if playback/recording is stopped or not yet started.\n"
		"StartTime: Is the real start time of the audio stream after start of playback/recording. If both, playback and "
		"recording are active (full-duplex mode), it is the start time of sound playback, ie an estimate of when the first "
		"sample hit the speakers. Same goes for pure playback. \n"
		"CaptureStartTime: Start time of audio capture (if any is active) - an estimate of when the first sound sample was "
		"captured. In pure capture mode, this is nearly identical to StartTime, but whenever playback is active, StartTime and "
		"CaptureStartTime will differ. CaptureStartTime doesn't take the user provided 'LatencyBias' into account.\n"
		"PositionSecs is an estimate of the current stream playback position in seconds, its not totally accurate, because "
		"it measures how much sound has been submitted to the sound system, not how much sound has left the "
		"speakers, i.e., it doesn't take driver and hardware latency into account.\n"
		"XRuns: Number of dropouts due to buffer overrun or underrun conditions.\n"
		"TotalCalls, TimeFailed and BufferSize are only for debugging of PsychPortAudio itself.\n"
		"CPULoad: How much load does the playback engine impose on the CPU? Values can range from 0.0 = 0% "
		"to 1.0 for 100%. Values close to 1.0 indicate that your system can't handle the load and timing glitches "
		"or sound glitches are likely. In such a case, try to reduce the load on your system.\n"
		"PredictedLatency: Is the latency in seconds of your driver+hardware combo. It tells you, "
		"how far ahead of time a sound device must be started ahead of the requested onset time via "
		"PsychPortAudio('Start'...) to make sure it actually starts playing in time. High quality systems like "
		"Linux or MacOS/X may allow values as low as 5 msecs or less on standard hardware. Other operating "
		"systems may require dozens or hundreds of milliseconds of headstart.\n"
		"LatencyBias: Is an additional bias setting you can impose via PsychPortAudio('LatencyBias', pahandle, bias); "
		"in case our drivers estimate is a bit off. Allows fine-tuning.\n"
		"SampleRate: Is the sampling rate for playback/recording in samples per second (Hz).\n"
		"RecordedSecs: Is the total amount of recorded sound data (in seconds) since start of capture.\n"
		"ReadSecs: Is the total amount of sound data (in seconds) that has been fetched from the internal buffer. "
		"The difference between RecordedSecs and ReadSecs is the amount of recorded sound data pending for retrieval. ";

	static char seeAlsoString[] = "Open GetDeviceSettings ";	 
	PsychGenericScriptType 	*status;
	const char *FieldNames[]={	"Active", "StartTime", "CaptureStartTime", "PositionSecs", "RecordedSecs", "ReadSecs", "XRuns", "TotalCalls", "TimeFailed", "BufferSize", "CPULoad", "PredictedLatency",
								"LatencyBias", "SampleRate"};
	int pahandle = -1;
	
	// Setup online help: 
	PsychPushHelp(useString, synopsisString, seeAlsoString);
	if(PsychIsGiveHelp()) {PsychGiveHelp(); return(PsychError_none); };
	
	PsychErrorExit(PsychCapNumInputArgs(1));     // The maximum number of inputs
	PsychErrorExit(PsychRequireNumInputArgs(1)); // The required number of inputs	
	PsychErrorExit(PsychCapNumOutputArgs(1));	 // The maximum number of outputs

	// Make sure PortAudio is online:
	PsychPortAudioInitialize();

	PsychCopyInIntegerArg(1, kPsychArgRequired, &pahandle);
	if (pahandle < 0 || pahandle>=MAX_PSYCH_AUDIO_DEVS || audiodevices[pahandle].stream == NULL) PsychErrorExitMsg(PsychError_user, "Invalid audio device handle provided.");

	PsychAllocOutStructArray(1, kPsychArgOptional, 1, 14, FieldNames, &status);
	
	PsychSetStructArrayDoubleElement("Active", 0, (audiodevices[pahandle].state >= 2) ? 1 : 0, status);
	PsychSetStructArrayDoubleElement("StartTime", 0, audiodevices[pahandle].startTime, status);
	PsychSetStructArrayDoubleElement("CaptureStartTime", 0, audiodevices[pahandle].captureStartTime, status);
	PsychSetStructArrayDoubleElement("PositionSecs", 0, ((double)(audiodevices[pahandle].playposition / audiodevices[pahandle].outchannels)) / (double) audiodevices[pahandle].streaminfo->sampleRate, status);
	PsychSetStructArrayDoubleElement("RecordedSecs", 0, ((double)(audiodevices[pahandle].recposition / audiodevices[pahandle].inchannels)) / (double) audiodevices[pahandle].streaminfo->sampleRate, status);
	PsychSetStructArrayDoubleElement("ReadSecs", 0, ((double)(audiodevices[pahandle].readposition / audiodevices[pahandle].inchannels)) / (double) audiodevices[pahandle].streaminfo->sampleRate, status);
	PsychSetStructArrayDoubleElement("XRuns", 0, audiodevices[pahandle].xruns, status);
	PsychSetStructArrayDoubleElement("TotalCalls", 0, audiodevices[pahandle].paCalls, status);
	PsychSetStructArrayDoubleElement("TimeFailed", 0, audiodevices[pahandle].noTime, status);
	PsychSetStructArrayDoubleElement("BufferSize", 0, audiodevices[pahandle].batchsize, status);
	PsychSetStructArrayDoubleElement("CPULoad", 0, Pa_GetStreamCpuLoad(audiodevices[pahandle].stream), status);
	PsychSetStructArrayDoubleElement("PredictedLatency", 0, audiodevices[pahandle].predictedLatency, status);
	PsychSetStructArrayDoubleElement("LatencyBias", 0, audiodevices[pahandle].latencyBias, status);
	PsychSetStructArrayDoubleElement("SampleRate", 0, audiodevices[pahandle].streaminfo->sampleRate, status);
	return(PsychError_none);
}

/* PsychPortAudio('Verbosity') - Set level of verbosity.
 */
PsychError PSYCHPORTAUDIOVerbosity(void) 
{
 	static char useString[] = "oldlevel = PsychPortAudio('Verbosity' [,level]);";
	static char synopsisString[] = 
		"Set level of verbosity for error/warning/status messages. 'level' optional, new level "
		"of verbosity. 'oldlevel' is the old level of verbosity. The following levels are "
		"supported: 0 = Shut up. 1 = Print errors, 2 = Print also warnings, 3 = Print also some info, "
		"4 = Print more useful info (default), >5 = Be very verbose (mostly for debugging the driver itself). ";		
	static char seeAlsoString[] = "Open GetDeviceSettings ";	 
	
	int level= -1;

	// Setup online help: 
	PsychPushHelp(useString, synopsisString, seeAlsoString);
	if(PsychIsGiveHelp()) {PsychGiveHelp(); return(PsychError_none); };
	
	PsychErrorExit(PsychCapNumInputArgs(1));     // The maximum number of inputs
	PsychErrorExit(PsychRequireNumInputArgs(0)); // The required number of inputs	
	PsychErrorExit(PsychCapNumOutputArgs(1));	 // The maximum number of outputs

	PsychCopyInIntegerArg(1, kPsychArgOptional, &level);
	if (level < -1) PsychErrorExitMsg(PsychError_user, "Invalid level of verbosity provided. Valid are levels of zero and greater.");
	
	// Return current/old level:
	PsychCopyOutDoubleArg(1, kPsychArgOptional, (double) verbosity);

	// Set new level, if one was provided:
	if (level > -1) verbosity = level;

	return(PsychError_none);
}

/* PsychPortAudio('LatencyBias') - Set a manual bias for the latencies we operate on.
 */
PsychError PSYCHPORTAUDIOLatencyBias(void) 
{
 	static char useString[] = "oldbias = PsychPortAudio('LatencyBias', pahandle [,biasSecs]);";
	static char synopsisString[] = 
		"Set audio output latency bias in seconds to 'biasSecs' and/or return old bias for a device "
		"'pahandle'. The device must be open for this setting to take effect. It is reset to zero at "
		"each reopening of the device. PsychPortAudio computes a latency value for the expected latency "
		"of an audio output device to get its timing right. If this latency value is slightly off for "
		"some reason, you can provide a bias value with this function to correct the computed value. "
		"See the online help for PsychPortAudio for more in depth explanation of latencies. ";		
	static char seeAlsoString[] = "Open GetDeviceSettings ";	 
	
	double bias= DBL_MAX;
	int pahandle = -1;

	// Setup online help: 
	PsychPushHelp(useString, synopsisString, seeAlsoString);
	if(PsychIsGiveHelp()) {PsychGiveHelp(); return(PsychError_none); };
	
	PsychErrorExit(PsychCapNumInputArgs(2));     // The maximum number of inputs
	PsychErrorExit(PsychRequireNumInputArgs(1)); // The required number of inputs	
	PsychErrorExit(PsychCapNumOutputArgs(1));	 // The maximum number of outputs

	// Make sure PortAudio is online:
	PsychPortAudioInitialize();

	PsychCopyInIntegerArg(1, kPsychArgRequired, &pahandle);
	if (pahandle < 0 || pahandle>=MAX_PSYCH_AUDIO_DEVS || audiodevices[pahandle].stream == NULL) PsychErrorExitMsg(PsychError_user, "Invalid audio device handle provided.");

	// Copy in optional new bias value:
	PsychCopyInDoubleArg(2, kPsychArgOptional, &bias);
	
	// Return current/old bias:
	PsychCopyOutDoubleArg(1, kPsychArgOptional, audiodevices[pahandle].latencyBias);

	// Set new bias, if one was provided:
	if (bias!=DBL_MAX) audiodevices[pahandle].latencyBias = bias;
	
	return(PsychError_none);
}

/* PsychPortAudio('GetDevices') - Enumerate all available sound devices.
 */
PsychError PSYCHPORTAUDIOGetDevices(void) 
{
 	static char useString[] = "devices = PsychPortAudio('GetDevices' [,devicetype]);";
	static char synopsisString[] = 
		"Returns 'devices', an array of structs, one struct for each available PortAudio device. "
		"Each struct contains information about its associated PortAudio device. The optional "
		"parameter 'devicetype' can be used to enumerate only devices of a specific class: \n"
		"1=Windows/DirectSound, 2=Windows/MME, 3=Windows/ASIO, 11=Windows/WDMKS, 13=Windows/WASAPI, "
		"8=Linux/ALSA, 7=Linux/OSS, 12=Linux/JACK, 5=MacOSX/CoreAudio.\n "
		"On OS/X you'll usually only see devices for the CoreAudio API, a first-class audio subsystem. "
		"On Linux you may have the choice between ALSA, JACK and OSS. ALSA or JACK provide very low "
		"latencies and very good timing, OSS is an older system which is less capable but not very "
		"widespread in use anymore. On MS-Windows you'll have the ''choice'' between up to 5 different "
		"audio subsystems: If you buy an expensive sound card with ASIO drivers, pick that API for low "
		"latency, it should give you comparable performance to OS/X or Linux. 2nd best choice "
		"would be WASAPI (on Windows-Vista) or WDMKS (on Windows-2000/XP) for ok latency on good days. DirectSound is the next "
		"worst choice if you have hardware with DirectSound support. If everything else fails, you'll be left "
		"with MMS, a premium example of system misdesign successfully sold to paying customers.";
		
	static char seeAlsoString[] = "Open GetDeviceSettings ";	 
	PsychGenericScriptType 	*devices;
	const char *FieldNames[]={	"DeviceIndex", "HostAudioAPIId", "HostAudioAPIName", "DeviceName", "NrInputChannels", "NrOutputChannels", 
								 		"LowInputLatency", "HighInputLatency", "LowOutputLatency", "HighOutputLatency",  "DefaultSampleRate", "xxx" };
	int devicetype = -1;
	int count = 0;
	int i, ic, filteredcount;
	PaDeviceInfo* padev = NULL;
	PaHostApiInfo* hainfo = NULL;
	
	// Setup online help: 
	PsychPushHelp(useString, synopsisString, seeAlsoString);
	if(PsychIsGiveHelp()) {PsychGiveHelp(); return(PsychError_none); };
	
	PsychErrorExit(PsychCapNumInputArgs(1));     // The maximum number of inputs
	PsychErrorExit(PsychRequireNumInputArgs(0)); // The required number of inputs	
	PsychErrorExit(PsychCapNumOutputArgs(1));	 // The maximum number of outputs

	// Make sure PortAudio is online:
	PsychPortAudioInitialize();

	PsychCopyInIntegerArg(1, kPsychArgOptional, &devicetype);
	if (devicetype < -1) PsychErrorExitMsg(PsychError_user, "Invalid 'devicetype' provided. Valid are levels of zero and greater.");
	
	// Query number of devices and allocate out struct array:
	count = (int) Pa_GetDeviceCount();
	if (count > 0) {
		filteredcount = count;
		if (devicetype!=-1) {
			filteredcount = 0;
			// Filtering bz host API requested: Do it.
			for (i=0; i<count; i++) {
				padev = Pa_GetDeviceInfo((PaDeviceIndex) i);
				hainfo = Pa_GetHostApiInfo(padev->hostApi);
				if (hainfo->type == devicetype) filteredcount++;
			}
		}

		PsychAllocOutStructArray(1, kPsychArgOptional, filteredcount, 11, FieldNames, &devices);
	}
	else {
		PsychErrorExitMsg(PsychError_user, "PTB-ERROR: PortAudio can't detect any supported sound device on this system.");
	}
	
	// Iterate through device list:
	ic = 0;
	for (i=0; i<count; i++) {
		padev = Pa_GetDeviceInfo((PaDeviceIndex) i);
		hainfo = Pa_GetHostApiInfo(padev->hostApi);
		if ((devicetype==-1) || (hainfo->type == devicetype)) {
			PsychSetStructArrayDoubleElement("DeviceIndex", ic, i, devices);
			PsychSetStructArrayDoubleElement("HostAudioAPIId", ic, hainfo->type, devices);
			PsychSetStructArrayStringElement("HostAudioAPIName", ic, hainfo->name, devices);
			PsychSetStructArrayStringElement("DeviceName", ic, padev->name, devices);
			PsychSetStructArrayDoubleElement("NrInputChannels", ic, padev->maxInputChannels, devices);
			PsychSetStructArrayDoubleElement("NrOutputChannels", ic, padev->maxOutputChannels, devices);
			PsychSetStructArrayDoubleElement("LowInputLatency", ic, padev->defaultLowInputLatency, devices);
			PsychSetStructArrayDoubleElement("HighInputLatency", ic, padev->defaultHighInputLatency, devices);
			PsychSetStructArrayDoubleElement("LowOutputLatency", ic, padev->defaultLowOutputLatency, devices);
			PsychSetStructArrayDoubleElement("HighOutputLatency", ic, padev->defaultHighOutputLatency, devices);
			PsychSetStructArrayDoubleElement("DefaultSampleRate", ic, padev->defaultSampleRate, devices);
			// PsychSetStructArrayDoubleElement("xxx", ic, 0, devices);
			ic++;
		}
	}
	
	return(PsychError_none);
}