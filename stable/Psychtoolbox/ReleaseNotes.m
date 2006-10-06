% Release notes for Psychtoolbox 3.0.7
%
% Psychtoolbox 3.0.7 conserves the state of the Psychtoolbox 'stable' branch
% as of Friday 6th October 2006. As of this date, the stable branch has
% not been updated with anything except a few bug fixes for a period of nine
% months.
%
% This version of Psychtoolbox only provides a few improvements over the
% last official release 1.0.6. It contains many  limitations as compared
% to the current 'beta' branch and will be made obsolete very soon by a
% big update of the 'stable' branch in a few days.
%
% We do not really recommend anybody to use this release. It is only here
% as a backup in case something should get severly broken during the following
% massive update of the 'stable' branch. This is pretty unlikely though.
%
% The following improvements have been made with respect to version 1.0.6
%
% - Bumped up the version number to 3.0.7: Now the most modern PTB has a
%   major version number of 3, distinguishing it clearly from the earlier
%   Windows- and MacOS-9 Psychtoolboxes with major version number 2.
%
% - Basic support for Quicktime movie playback on OS-X. The Quicktime
%   engine in this release works well, but the QT engine of PTB 3.0.8 is
%   significantly better.
%
% - Screen('PreloadTextures') command for prefetching of textures.
%
% - OpenGL low-level function support. Currently implemented:
%   glPushMatrix, glPopMatrix, glLoadIdentitiy, glScale, glTranslate
%   and glRotate.
%
% - Screen Arc drawing functions.
% 
% - Minor bug fixes all over the place. Still, PTB 3.0.8 will have many
%   more bugs fixed.
%
% - A new GetChar implementation based on Cocoa. Works better than the old
%   one, but is still very fragile and prone to malfunction and crashes.
%
% Enjoy! Or better: Ignore this release!
