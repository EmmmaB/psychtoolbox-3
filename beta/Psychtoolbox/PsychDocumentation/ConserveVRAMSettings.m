% ConserveVRAMSettings: Workaround for flawed hardware and drivers
%
% The command Screen('Preference', 'ConserveVRAM', mode); can be used to
% enable a couple of special work-arounds inside Screen to work around
% broken operating systems, graphics drivers or graphics hardware, or to
% work around ressource limitations of graphics hardware.
%
% You define the requested workaround by setting the parameter 'mode' to a
% sum of the following values:
%
% Allowerd summands (flags) for 'mode' and their effect:
%
% 1 == kPsychDisableAUXBuffers: A setting of 1 asks Psychtoolbox to not
% allocate any OpenGL AUXiliary buffers when opening a new onscreen window.
% AUX buffers are only needed if you want to run the Screen('Flip') command
% with the optional argument 'dontclear = 1' and you are not using the
% imaging pipeline, or if you want to use stereomode 2 or 3 without using
% the imaging pipeline. If you do use the imaging pipeline or don't use any
% of the above, there's no need for AUX buffers.
%
% This setting is mostly meant to save a bit of VRAM on graphics hardware
% that only has very small amounts of VRAM, e.g., only 16 MB or 8 MB VRAM.
%
%
% 2 == kPsychDontCacheTextures: A setting of 2 asks Psychtoolbox not to
% cache used textures in the graphics hardware local VRAM memory. This will
% save some VRAM memory at the expense of lower drawing performance. Only
% useful on gfx-hardware with low amounts of VRAM and only works on
% MacOS/X. The flag is silently ignored on Windows and Linux.
%
%
% 4 == kPsychOverrideWglChoosePixelformat: This is a workaround for broken
% MS-Windows graphics drivers: Ask Screen to not use the
% wglChoosePixelFormat() command when creating a new onscreen window. This
% can prevent crashes on such broken setups, but it will disable OpenGL
% multisampling for anti-aliasing, ie., the 'multisample' parameter of
% Screen('OpenWindow') will be ignored. In the future, other special
% capabilities will be disabled as well.
%
%
% 8 == kPsychDisableContextIsolation: This is a workaround for broken
% MS-Windows graphics drivers: Do not create separate isolated OpenGL
% rendering contexts for Screen and MOGL when using low level OpenGL 3D
% graphics commands with OpenGL for Matlab. This prevents crashes on broken
% setups, but debugging of your own 3D code may become much harder. Its
% better to upgrade to the latest fixed drivers.
% Before you try this setting 8, first try if the setting 256 (see below)
% fixes the problem for you. That is a softer approach - If it works for
% you then you won't lose any important functionality!
%
%
% 16 == kPsychDontAttachStencilToFBO: Do not attach stencil buffer
% attachments to OpenGL framebuffer objects when using OpenGL 3D graphics
% in conjunction with the Psychtoolbox imaging pipeline. This is again a
% workaround for some broken MS-Windows graphics drivers to make the 3D +
% imaging combo work at least when no stencil buffer is needed.
%
% 32 == kPsychDontShareContextRessources: Do not share ressources between
% different onscreen windows. Usually you want PTB to share all ressources
% like offscreen windows, textures and GLSL shaders among all open onscreen
% windows. If that causes trouble for some weird reason, you can prevent
% automatic sharing with this flag.
%
% 64 == kPsychUseSoftwareRenderer: Request use of a software implemented
% renderer instead of the GPU hardware renderer. This request is silently
% ignored if your platform doesn't support software rendering. Currently
% only MacOS/X 10.4 and later in windowed mode (i.e. not fullscreen)
% supports this via the Apple floating point renderer. Mostly useful for
% testing and debugging of scripts that need floating point support on
% hardware that doesn't support this. Not generally useful for production
% use.
%
% 128 == kPsychEnforceForegroundWindow: Request application of the Windows
% GDI calls SetForegroundWindow() and SetFocus() on each created onscreen
% window on MS-Windows. This may improve reliabilty of onscreen windows
% staying in front of all other windows, but is incompatible with the use
% of GetChar, CharAvail and ListenChar, so it must be requested with this
% flag.
%
% 256 == kPsychUseWindowsContextSharingWorkaround1
% On MS-Windows, skip a few not too essential setup steps when creating a
% userspace OpenGL rendering context for 3D mode. This is a "soft" version
% of kPsychDisableContextIsolation -- Less intrusive as it doesn't disable
% context isolation completely, but only a subset. May be able to
% work-around an NVidia driver bug reported in March 2008 on GF8xxx series.
%
% 512 == kPsychAvoidCPUGPUSync: Avoid any internal calls (if possible) that
% could cause a synchronization of the CPU and GPU. Synchronization is a
% potentially expensive operation that can degrade performance in certain
% circumstances. Its often needed for error checking. Setting this flag may
% give you a speedup on certain operations, but at the cost of reduced
% error checking and error handling: Error conditions detected otherwise
% may silently slip through and cause mysterious malfunctions or stimulus
% corruption without PTB noticing this or providing any troubleshooting
% tips. The usefulness of this flag highly depends on your graphics
% hardware, driver and operating system. It may give a large speedup, or no
% speedup at all, but it will always reduce robustness!
%
% --> It's always better to update your graphics drivers with fixed
% versions or buy proper hardware than using these workarounds. They are
% meant as a last ressort, e.g., if you need to get something going quickly
% or can't get access to bug-fixed drivers.
%