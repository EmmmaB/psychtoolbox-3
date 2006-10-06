function PsychtoolboxPostInstallRoutine(isUpdate, flavor)
% PsychtoolboxPostInstallRoutine(isUpdate [, flavor])
%
% Psychtoolbox post installation routine. You should not call this
% function directly! This routine is called by DownloadPsychtoolbox,
% or UpdatePsychtoolbox after a successfull download/update of
% Psychtoolbox. The routine performs tasks that are common to
% downloads and updates, so they can share their code/implementation.
%
% As PsychtoolboxPostInstallRoutine itself is downloaded or updated,
% it can contain code specific to each Psychtoolbox revision/release
% to perform special setup procedures for new features, to announce
% important info to the user, whatever...
%
% Currently the routine performs the following tasks:
%
% 1. Clean up the Matlab path to Psychtoolbox: Remove unneeded .svn subfolders.
% 2. Contact the Psychtoolbox server to perform online registration of this
%    working copy of Psychtoolbox.

%
% History:
% 23/06/2006 Written (MK).
% 17/09/2006 Made working on Matlab-5 and Octave. Made more robust. (MK)
% 22/09/2006 Replace system copy commands by Matlabs copyfile() - More
%            robust (MK).

fprintf('\n\nRunning post-install routine...\n\n');

if nargin < 1
   error('PsychtoolboxPostInstallRoutine: Required argument isUpdate missing!');
end;

if nargin < 2
    % No flavor provided: Default to 'unknown', but try to determine it from the
    % flavor file if this is an update.
    flavor = 'unknown';
    try
        if isUpdate>0
            % This is an update of an existing working copy. Check if flavor-file
            % is available:
            flavorfile = [PsychtoolboxRoot 'ptbflavorinfo.txt'];
            if exist(flavorfile, 'file')
                fd=fopen(flavorfile);
                if fd > -1
                    flavor = fscanf(fd, '%s');
                    fclose(fd);
                end
            end
        end
    catch
        fprintf('Info: Failed to determine flavor of this Psychtoolbox. Not a big deal...\n');
    end
else
    % Flavor provided: Write it into the flavor file for use by later update calls:
    try
        flavorfile = [PsychtoolboxRoot 'ptbflavorinfo.txt'];
        fd=fopen(flavorfile, 'wt');
        if fd > -1
            fprintf(fd, '%s\n', flavor);
            fclose(fd);
        end
    catch
        fprintf('Info: Failed to store flavor of this Psychtoolbox to file. Not a big deal...\n');
    end
end

% Get rid of any remaining .svn folders in the path.
try
    path(RemoveSVNPaths);
    if exist('savepath')
        savepath;
    else
        path2rc;
    end
catch
    fprintf('Info: Failed to remove .svn subfolders from path. Not a big deal...\n');
end

% Try to execute online registration routine: This should be fail-safe in case
% of no network connection.
fprintf('\n\n');
PsychtoolboxRegistration(isUpdate, flavor);
fprintf('\n\n\n');

% Some goodbye, copyright and getting started blurb...
fprintf('\nDone with post-installation. Psychtoolbox is ready for use.\n');
fprintf('Psychtoolbox is free software; you can redistribute it and/or modify\n');
fprintf('it under the terms of the GNU General Public License as published by\n');
fprintf('the Free Software Foundation; either version 2 of the License, or\n');
fprintf('(at your option) any later version. See the file ''License.txt'' in\n');
fprintf('the Psychtoolbox root folder for exact licensing conditions.\n\n');

fprintf('If you are new to the Psychtoolbox, you might try this: \nhelp Psychtoolbox\n\n');
fprintf('Useful Psychtoolbox websites:\n');
fprintf('web http://www.psychtoolbox.org -browser\n');
fprintf('web http://en.wikibooks.org/wiki/Matlab:Psychtoolbox -browser\n');
fprintf('Archive of Psychtoolbox announcements:\n');
fprintf('web http://lists.berlios.de/pipermail/osxptb-announce/  -browser\n');

fprintf('\nEnjoy!\n\n');

% Clear out everything:
clear all;

return;
