function makeitOnMPIHDRviaFunky(what)

dos('copy T:\projects\OpenGLPsychtoolbox\trunk\PsychSourceGL\Source\Common\Base\PsychScriptingGlue.cc T:\projects\OpenGLPsychtoolbox\trunk\PsychSourceGL\Source\Common\Base\PsychScriptingGlue.c');

if nargin < 1
   what = 0;
end

if what == 0
   % Default: Build Screen.mexw32
   mex -v -outdir T:\projects\OpenGLPsychtoolbox\trunk\PsychSourceGL\Projects\Windows\build\ -output Screen.mexw32 -DPTBMODULE_Screen -DPTBVIDEOCAPTURE_ARVIDEO -DPTBVIDEOCAPTURE_QT -DTARGET_OS_WIN32 -ID:\install\QuickTimeSDK\CIncludes -I"C:\Programme\Microsoft Visual Studio 8\VC\Include" -I"C:\Program Files\Microsoft DirectX SDK\Include" -ICommon\Base -ICommon\Screen -IWindows\Base -IWindows\Screen -I..\Cohorts\ARToolkit\include Windows\Screen\*.c Windows\Base\*.c Common\Base\*.c Common\Screen\*.c kernel32.lib user32.lib gdi32.lib advapi32.lib glu32.lib opengl32.lib qtmlClient.lib ddraw.lib winmm.lib delayimp.lib libARvideo.lib
   dos('copy T:\projects\OpenGLPsychtoolbox\trunk\PsychSourceGL\Projects\Windows\build\Screen.mexw32 T:\projects\OpenGLPsychtoolbox\trunk\Psychtoolbox\PsychBasic\MatlabWindowsFilesR2007a\');
end

if what == 1
   % Build WaitSecs.mexw32
   mex -v -outdir T:\projects\OpenGLPsychtoolbox\trunk\PsychSourceGL\Projects\Windows\build\ -output WaitSecs.mexw32 -DPTBMODULE_WaitSecs -I"C:\Programme\Microsoft Visual Studio 8\VC\Include" -ICommon\Base -ICommon\WaitSecs -IWindows\Base Windows\Base\*.c Common\Base\*.c Common\WaitSecs\*.c kernel32.lib user32.lib winmm.lib
   dos('copy T:\projects\OpenGLPsychtoolbox\trunk\PsychSourceGL\Projects\Windows\build\WaitSecs.mexw32 T:\projects\OpenGLPsychtoolbox\trunk\Psychtoolbox\PsychBasic\MatlabWindowsFilesR2007a\');
end

if what == 2
   % Build PsychPortAudio.mexw32
   mex -v -outdir T:\projects\OpenGLPsychtoolbox\trunk\PsychSourceGL\Projects\Windows\build\ -output PsychPortAudio.mexw32 -DPTBMODULE_PsychPortAudio -I"C:\Programme\Microsoft Visual Studio 8\VC\Include" -ICommon\Base -ICommon\PsychPortAudio -IWindows\Base Windows\Base\*.c Common\Base\*.c Common\PsychPortAudio\*.c kernel32.lib user32.lib winmm.lib portaudio_x86.lib
   dos('copy T:\projects\OpenGLPsychtoolbox\trunk\PsychSourceGL\Projects\Windows\build\PsychPortAudio.mexw32 T:\projects\OpenGLPsychtoolbox\trunk\Psychtoolbox\PsychBasic\MatlabWindowsFilesR2007a\');
end

if what == 3
   % Build GetSecs.mexw32
   mex -v -outdir T:\projects\OpenGLPsychtoolbox\trunk\PsychSourceGL\Projects\Windows\build\ -output GetSecs.mexw32 -DPTBMODULE_GetSecs -I"C:\Programme\Microsoft Visual Studio 8\VC\Include" -ICommon\Base -ICommon\GetSecs -IWindows\Base Windows\Base\*.c Common\Base\*.c Common\GetSecs\*.c kernel32.lib user32.lib winmm.lib
   dos('copy T:\projects\OpenGLPsychtoolbox\trunk\PsychSourceGL\Projects\Windows\build\GetSecs.mexw32 T:\projects\OpenGLPsychtoolbox\trunk\Psychtoolbox\PsychBasic\MatlabWindowsFilesR2007a\');
end

if what == 4
   % Build IOPort.mexw32
   mex -v -outdir T:\projects\OpenGLPsychtoolbox\trunk\PsychSourceGL\Projects\Windows\build\ -output IOPort.mexw32 -DPTBMODULE_IOPort -I"C:\Programme\Microsoft Visual Studio 8\VC\Include" -ICommon\Base -ICommon\IOPort -IWindows\Base -IWindows\IOPort Windows\Base\*.c Common\Base\*.c Common\IOPort\*.c Windows\IOPort\*.c kernel32.lib user32.lib winmm.lib
   dos('copy T:\projects\OpenGLPsychtoolbox\trunk\PsychSourceGL\Projects\Windows\build\IOPort.mexw32 T:\projects\OpenGLPsychtoolbox\trunk\Psychtoolbox\PsychBasic\MatlabWindowsFilesR2007a\');
end

if what == 5
   % Build PsychCV.mexw32
   mex -v -outdir T:\projects\OpenGLPsychtoolbox\trunk\PsychSourceGL\Projects\Windows\build\ -output PsychCV.mexw32 -DPTBMODULE_PsychCV -DTARGET_OS_WIN32 -ID:\install\QuickTimeSDK\CIncludes -I"C:\Programme\Microsoft Visual Studio 8\VC\Include" -I"C:\Program Files\Microsoft DirectX SDK\Include" -ICommon\Base -ICommon\PsychCV -IWindows\Base -I..\Cohorts\ARToolkit\include Windows\Base\*.c Common\Base\*.c Common\PsychCV\*.c kernel32.lib user32.lib gdi32.lib advapi32.lib glu32.lib opengl32.lib winmm.lib delayimp.lib libARvideo.lib libARgsub.lib libARgsub_lite.lib libARgsubUtil.lib libARMulti.lib libAR.lib 
   dos('copy T:\projects\OpenGLPsychtoolbox\trunk\PsychSourceGL\Projects\Windows\build\PsychCV.mexw32 T:\projects\OpenGLPsychtoolbox\trunk\Psychtoolbox\PsychBasic\MatlabWindowsFilesR2007a\');
end

delete('T:\projects\OpenGLPsychtoolbox\trunk\PsychSourceGL\Source\Common\Base\PsychScriptingGlue.c');
return;
