function linuxmakeit_ubuntugutsy(mode)
% This is the GNU/Linux version of makeit to build the Linux
% version of PTB's Screen - command.
% This version is adapted to build on the "funky" Laptop under Ubuntu Linux 7.1 Gutsy against Matlab R2007a and later 
if nargin < 1
    mode = 0
end;

if mode==0
    % Build Screen.mexglx:
    mex CFLAGS='$CFLAGS -std=gnu99' -v -outdir ../Projects/Linux/build/ -output Screen -DPTBVIDEOCAPTURE_LIBDC -I/usr/X11R6/include -ICommon/Base -ICommon/Screen -ILinux/Base -ILinux/Screen -L/usr/X11R6/lib Common/Base/*.cc Linux/Base/*.c Linux/Screen/*.c Common/Screen/*.c Common/Base/*.c -lc -lrt -ldl -lGL -lX11 -lXext /usr/lib/libXxf86vm.a /usr/lib/libGLU.a /usr/lib/libdc1394.a /usr/lib/libraw1394.a /usr/lib/libusb-1.0.a 
    unix('cp /home/kleinerm/projects/OpenGLPsychtoolbox/trunk/PsychSourceGL/Projects/Linux/build/Screen.mexglx /home/kleinerm/projects/OpenGLPsychtoolbox/trunk/Psychtoolbox/PsychBasic/');
end;

if mode==1
    % Build GetSecs.mexglx:
    mex CFLAGS='$CFLAGS -std=gnu99' -v -outdir ../Projects/Linux/build/ -output GetSecs -DPTBMODULE_GetSecs -ICommon/Base -ILinux/Base -ICommon/GetSecs -ICommon/Screen Common/Base/*.cc Linux/Base/*.c Common/Base/*.c Common/GetSecs/*.c -lc -lrt 
    unix('cp /home/kleinerm/projects/OpenGLPsychtoolbox/trunk/PsychSourceGL/Projects/Linux/build/GetSecs.mexglx /home/kleinerm/projects/OpenGLPsychtoolbox/trunk/Psychtoolbox/PsychBasic/');
end;

if mode==2
    % Build WaitSecs.mexglx:
    mex CFLAGS='$CFLAGS -std=gnu99' -v -outdir ../Projects/Linux/build/ -output WaitSecs -DPTBMODULE_WaitSecs -ICommon/Base -ILinux/Base -ICommon/WaitSecs -ICommon/Screen Common/Base/*.cc Linux/Base/*.c Common/Base/*.c Common/WaitSecs/*.c -lc -lrt 
    unix('cp /home/kleinerm/projects/OpenGLPsychtoolbox/trunk/PsychSourceGL/Projects/Linux/build/WaitSecs.mexglx /home/kleinerm/projects/OpenGLPsychtoolbox/trunk/Psychtoolbox/PsychBasic/');
end;

if mode==3
    % Build PsychPortAudio.mexglx:
    mex CFLAGS='$CFLAGS -std=gnu99' -v -outdir ../Projects/Linux/build/ -output PsychPortAudio -DPTBMODULE_PsychPortAudio -ICommon/Base -ILinux/Base -ICommon/PsychPortAudio -ICommon/Screen Common/Base/*.cc Linux/Base/*.c Common/Base/*.c Common/PsychPortAudio/*.c /usr/local/lib/libportaudio.a -lc -lrt -lasound
    unix('cp /home/kleinerm/projects/OpenGLPsychtoolbox/trunk/PsychSourceGL/Projects/Linux/build/PsychPortAudio.mexglx /home/kleinerm/projects/OpenGLPsychtoolbox/trunk/Psychtoolbox/PsychBasic/');
end

if mode==4
    % Build Eyelink.mexglx:
    mex CFLAGS='$CFLAGS -std=gnu99' -v -outdir ../Projects/Linux/build/ -output Eyelink -DPTBMODULE_Eyelink -ICommon/Base -ILinux/Base -ICommon/Eyelink -ICommon/Screen Common/Base/*.cc Linux/Base/*.c Common/Base/*.c Common/Eyelink/*.c -leyelink_core -lc -lrt
    unix('cp /home/kleinerm/projects/OpenGLPsychtoolbox/trunk/PsychSourceGL/Projects/Linux/build/Eyelink.mexglx /home/kleinerm/projects/OpenGLPsychtoolbox/trunk/Psychtoolbox/PsychBasic/');    
end

if mode==5
    % Build IOPort.mexglx:
    mex CFLAGS='$CFLAGS -std=gnu99' -v -outdir ../Projects/Linux/build/ -output IOPort -DPTBMODULE_IOPort -ICommon/Base -ILinux/Base -ICommon/IOPort -ICommon/Screen Common/Base/*.cc Linux/Base/*.c Common/Base/*.c Common/IOPort/*.c -lc -lrt
    unix('cp /home/kleinerm/projects/OpenGLPsychtoolbox/trunk/PsychSourceGL/Projects/Linux/build/IOPort.mexglx /home/kleinerm/projects/OpenGLPsychtoolbox/trunk/Psychtoolbox/PsychBasic/');    
end

return;
