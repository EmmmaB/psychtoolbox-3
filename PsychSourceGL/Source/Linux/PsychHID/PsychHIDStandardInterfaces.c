/*
  PsychToolbox3/Source/Linux/PsychHID/PsychHIDStandardInterfaces.c
  
  PROJECTS: PsychHID only.
  
  PLATFORMS:  Linux.
  
  AUTHORS:
  mario.kleiner@tuebingen.mpg.de    mk

  HISTORY:
  27.07.2011     mk     Created.
  
  TO DO:

*/

#include "PsychHIDStandardInterfaces.h"

static Display *dpy = NULL;
static int xi_opcode, event, error;
static XIDeviceInfo *info = NULL;
static int ndevices = 0;
static int masterDevice = -1;

void PsychHIDInitializeHIDStandardInterfaces(void)
{
	int major, minor;
	int rc, i;

	// We must initialize XLib for multi-threaded operations / access on first
	// call:
	// TODO FIXME: We can only do this on Octave for now, not on Matlab!
	// Matlab uses XLib long before we get a chance to get here, but XInitThreads()
	// must be called as very first XLib function after process startup or bad things
	// will happen! So, we can't call it...
	// Because some system configurations can't handle multi-threaded x at all,
	// we allow users to opt-out of this if they define an environment variable
	// PSYCHTOOLBOX_SINGLETHREADEDX.
	#ifdef PTBOCTAVE3MEX
	if (NULL == getenv("PSYCHTOOLBOX_SINGLETHREADEDX")) XInitThreads();
	#endif

	// Open our own private X-Display connection for HID handling:
	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		PsychErrorExitMsg(PsychError_system, "PsychHID: FATAL ERROR: Couldn't open default X11 display connection to X-Server! Game over!");
	}

	// XInputExtension supported? If so do basic init:
	if (!XQueryExtension(dpy, "XInputExtension", &xi_opcode, &event, &error)) {
		printf("PsychHID: ERROR: XINPUT extension unsupported!\n");
		goto out;
	}

	// XInput V2 supported?
	major = 2;
	minor = 0;
	rc = XIQueryVersion(dpy, &major, &minor);
	if (rc == BadRequest) {
		printf("PsychHID: ERROR: No XI2 support. Server supports version %d.%d only.\n", major, minor);
		goto out;
	} else if (rc != Success) {
		printf("PsychHID: ERROR: Internal Error! This is a bug in Xlib.\n");
		goto out;
	}

	// printf("PsychHID: INFO: XI2 supported. Server provides version %d.%d.\n", major, minor);

	// Enumerate all XI2 input devices:
	info = XIQueryDevice(dpy, XIAllDevices, &ndevices);

	masterDevice = -1;
	for(i = 0; i < ndevices; i++) {
		if (info[i].use == XIMasterPointer) {
			masterDevice = i;
			break;
		}
	}
	if (masterDevice == -1) printf("PsychHID: WARNING! No master keyboard/pointer found! This will end badly...\n");

	// Switch X-Server connection into synchronous mode: We need this to get
	// a higher timing precision.
	XSynchronize(dpy, TRUE);

	return;

out:
	XCloseDisplay(dpy);
	dpy = NULL;
	PsychErrorExitMsg(PsychError_system, "PsychHID: FATAL ERROR: X Input extension version 2.0 or later not available! Game over!");	
}

void PsychHIDShutdownHIDStandardInterfaces(void)
{
	// Release list of enumerated input devices:
	XIFreeDeviceInfo(info);
	info = NULL;

	// Close our dedicated x-display connection and we are done:
	XCloseDisplay(dpy);
	dpy = NULL;

	return;
}

PsychError PsychHIDEnumerateHIDInputDevices(int deviceClass)
{
    const char *deviceFieldNames[]={"usagePageValue", "usageValue", "usageName", "index", "transport", "vendorID", "productID", "version", 
                                    "manufacturer", "product", "serialNumber", "locationID", "interfaceID", "totalElements", "features", "inputs", 
                                    "outputs", "collections", "axes", "buttons", "hats", "sliders", "dials", "wheels"};
    int numDeviceStructElements, numDeviceStructFieldNames=24, deviceIndex;
    PsychGenericScriptType	*deviceStruct;
    XIDeviceInfo *dev;
    int i;
    char *type = "";

    // Preparse: Count matching devices for deviceClass
    numDeviceStructElements = 0;
    for(i = 0; i < ndevices; i++) {
        dev = &info[i];
	if ((int) (dev->use) == deviceClass) numDeviceStructElements++;
    }

    // Alloc struct array of sufficient size:
    PsychAllocOutStructArray(1, FALSE, numDeviceStructElements, numDeviceStructFieldNames, deviceFieldNames, &deviceStruct);
    deviceIndex = 0;

    // Return info:
    for(i = 0; i < ndevices; i++) {
	// Check i'th device:
        dev = &info[i];

	// Skip if non matching class:
	if ((int) (dev->use) != deviceClass) continue;

        switch(dev->use) {
            case XIMasterPointer: type = "master pointer"; break;
            case XIMasterKeyboard: type = "master keyboard"; break;
            case XISlavePointer: type = "slave pointer"; break;
            case XISlaveKeyboard: type = "slave keyboard"; break;
            case XIFloatingSlave: type = "floating slave"; break;
        }

	// Usagepage is 1 for "Desktop usage page":
        PsychSetStructArrayDoubleElement("usagePageValue",	deviceIndex, 	(double) 1, deviceStruct);

	if (dev->use == XIMasterKeyboard || dev->use == XISlaveKeyboard) {
		// Usage 6 is for keyboard:
		PsychSetStructArrayDoubleElement("usageValue",	deviceIndex, (double) 6, deviceStruct);
	}
	else if (dev->use != XIFloatingSlave) {
		// Usage is 2 for mouse:
		PsychSetStructArrayDoubleElement("usageValue",	deviceIndex, (double) 2, deviceStruct);
	} else {
		// Assign meaningless Usage of zero for floating slave devices:
		PsychSetStructArrayDoubleElement("usageValue",	deviceIndex, (double) 0, deviceStruct);
	}

        PsychSetStructArrayStringElement("usageName",		deviceIndex, 	type, deviceStruct);
        PsychSetStructArrayDoubleElement("index",		deviceIndex, 	(double) i, deviceStruct);
        PsychSetStructArrayStringElement("transport",		deviceIndex, 	(dev->enabled) ? "enabled" : "disabled", deviceStruct);
        PsychSetStructArrayStringElement("product",		deviceIndex, 	dev->name, deviceStruct);
        PsychSetStructArrayDoubleElement("locationID",		deviceIndex, 	(double) dev->attachment, deviceStruct);
        PsychSetStructArrayDoubleElement("interfaceID",		deviceIndex, 	(double) dev->deviceid, deviceStruct);

        //PsychSetStructArrayDoubleElement("vendorID",		deviceIndex, 	(double)currentDevice->vendorID, 	deviceStruct);
        //PsychSetStructArrayDoubleElement("productID",		deviceIndex, 	(double)currentDevice->productID, 	deviceStruct);
        //PsychSetStructArrayDoubleElement("version",		deviceIndex, 	(double)currentDevice->version, 	deviceStruct);
        //PsychSetStructArrayStringElement("manufacturer",	deviceIndex, 	currentDevice->manufacturer, 		deviceStruct);
        //PsychSetStructArrayStringElement("serialNumber",	deviceIndex, 	currentDevice->serial, 			deviceStruct);
        //PsychSetStructArrayDoubleElement("totalElements",	deviceIndex, 	(double)currentDevice->totalElements, 	deviceStruct);
        //PsychSetStructArrayDoubleElement("features",		deviceIndex, 	(double)currentDevice->features, 	deviceStruct);
        //PsychSetStructArrayDoubleElement("inputs",		deviceIndex, 	(double)currentDevice->inputs, 		deviceStruct);
        //PsychSetStructArrayDoubleElement("outputs",		deviceIndex, 	(double)currentDevice->outputs, 	deviceStruct);
        //PsychSetStructArrayDoubleElement("collections",	deviceIndex, 	(double)currentDevice->collections, 	deviceStruct);
        //PsychSetStructArrayDoubleElement("axes",		deviceIndex, 	(double)currentDevice->axis, 		deviceStruct);
        //PsychSetStructArrayDoubleElement("buttons",		deviceIndex, 	(double)currentDevice->buttons, 	deviceStruct);
        //PsychSetStructArrayDoubleElement("hats",		deviceIndex, 	(double)currentDevice->hats, 		deviceStruct);
        //PsychSetStructArrayDoubleElement("sliders",		deviceIndex, 	(double)currentDevice->sliders, 	deviceStruct);
        //PsychSetStructArrayDoubleElement("dials",		deviceIndex, 	(double)currentDevice->dials, 		deviceStruct);
        //PsychSetStructArrayDoubleElement("wheels",		deviceIndex, 	(double)currentDevice->wheels, 		deviceStruct);
	
	deviceIndex++;
    }
}

PsychError PsychHIDOSKbCheck(int deviceIndex, double* scanList)
{
	PsychNativeBooleanType* buttonStates;
	unsigned char keys_return[32];
	int keysdown;
	double timestamp;
	int i, j, dummy1, nclasses;
	XIDeviceInfo *dev = NULL;
	XIAnyClassInfo *classes;

	// Map "default" deviceIndex to legace "Core protocol" method of querying keyboard
	// state. This will give us whatever X has setup as default keyboard:
	if (deviceIndex == INT_MAX) {
		// Request current keyboard state from X-Server:
		XQueryKeymap(dpy, keys_return);
	}
	else {
		// Non-Default deviceIndex: Want to query specific slave keyboard.
		// Validate it maps to a slave keyboard device, as we can't handle
		// master keyboard devices this way and don't want to touch anything
		// but a keyboard'ish device:
		if (deviceIndex < 0 || deviceIndex >= ndevices || info[deviceIndex].use != XISlaveKeyboard) {
			PsychErrorExitMsg(PsychError_user, "Invalid keyboard deviceIndex specified. No such device or not a physical slave keyboard device!");
		}

		// Open connection to slave keyboard device:
		XDevice* mydev = XOpenDevice(dpy, (XID) info[deviceIndex].deviceid);

		// Query its current state:
		XDeviceState* state = XQueryDeviceState(dpy, mydev);

		// printf("Dummy = %i , NClasses = %i\n", dummy1, state->num_classes);

		// Find state structure with key status info:
		for (i = 0; i < state->num_classes; i++) {
			// printf("Class %i: Type %i - %i\n", i, (int) state->data[i].class, (int) state->data[i].length);
			if (state->data[i].class == KeyClass) {
				// printf("NumKeys %i\n", ((XKeyState*) &(state->data[i]))->num_keys);

				// Copy 32 Byte keystate vector into key_return. Each bit encodes for one key:
				memcpy(&keys_return[0], &(((XKeyState*) &(state->data[i]))->keys[0]), sizeof(keys_return));
			}

			//		if (dev->classes[i]->type == 1) printf("NumButtons %i\n", ((XIButtonClassInfo*) dev->classes[i])->num_buttons);
			//		if (dev->classes[i]->type == 2) printf("Value %f\n", (float) ((XIValuatorClassInfo*) dev->classes[i])->value);
		}

		XFreeDeviceState(state);
		XCloseDevice(dpy, mydev);
	}

	// Done with query. We have keyboard state in keys_return[] now.

	// Request current time of query:
	PsychGetAdjustedPrecisionTimerSeconds(&timestamp);

	// Reset overall key state to "none pressed":
	keysdown = 0;

	// Any key down?
	for (i = 0; i < 32; i++) keysdown+=(unsigned int) keys_return[i];

	// Copy out overall keystate:
	PsychCopyOutDoubleArg(1, kPsychArgOptional, (keysdown > 0) ? 1 : 0);

	// Copy out timestamp:
	PsychCopyOutDoubleArg(2, kPsychArgOptional, timestamp);

	// Copy keyboard state:
	PsychAllocOutBooleanMatArg(3, kPsychArgOptional, 1, 256, 1, &buttonStates);

	// Map 32 times 8 bitvector to 256 element return vector:
	for(i = 0; i < 32; i++) {
		for(j = 0; j < 8; j++) {
			// This key down?
			buttonStates[i*8 + j] = (PsychNativeBooleanType) (keys_return[i] & (1<<j)) ? 1 : 0;
			// Apply scanList mask, if any provided:
			if (scanList && (scanList[i*8 + j] <= 0)) buttonStates[i*8 + j] = (PsychNativeBooleanType) 0;
		}
	}

	return(PsychError_none);
}

/*
//	dev = XIQueryDevice(dpy, info[deviceIndex].deviceid, &dummy1);

	printf("Dummy = %i , NClasses = %i\n", dummy1, dev->num_classes);
	for (i = 0; i < dev->num_classes; i++) {
		printf("Class %i: Type %i\n", i, (int) dev->classes[i]->type);
		if (dev->classes[i]->type == 0) printf("NumKeys %i\n", ((XIKeyClassInfo*) dev->classes[i])->num_keycodes);
		if (dev->classes[i]->type == 1) printf("NumButtons %i\n", ((XIButtonClassInfo*) dev->classes[i])->num_buttons);
		if (dev->classes[i]->type == 2) printf("Value %f\n", (float) ((XIValuatorClassInfo*) dev->classes[i])->value);
	}

	XIFreeDeviceInfo(dev);
*/
