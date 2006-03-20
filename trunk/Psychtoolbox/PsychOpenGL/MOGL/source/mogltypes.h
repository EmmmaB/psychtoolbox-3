#ifndef _MOGLTYPE_H
#define _MOGLTYPE_H

/*
 * mogltype.h -- common definitions for gl/glu and glm modules
 *
 * 09-Dec-2005 -- created (RFM)
 *
*/

#include "mex.h"

/* glew.h is part of GLEW library for automatic detection and binding of
   OpenGL core functionality and extensions.
 */
#include "glew.h"

/* Includes specific to MacOS-X version of mogl: */
#ifdef MACOSX

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>
#include <AGL/agl.h>

#endif

/* Includes specific to GNU/Linux version of mogl: */
#ifdef LINUX

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "glxew.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

#endif

/* Includes specific to M$-Windows version of mogl: */
#ifdef WINDOWS
#include <stdlib.h>
#include <string.h>
#include "wglew.h"
#endif

// typedef for command map entries
typedef struct cmdhandler {
    char *cmdstr;
    void (*cmdfn)(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]);
} cmdhandler;

#endif
