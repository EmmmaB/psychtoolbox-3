function FindStringInFile(filename,string)% FindStringInFile(filename,string)%%	Finds and prints lines containing the string in the file.%%	Denis Pelli 5/14/96%% 5/15/96  dhb  Wrote from Pelli's SearchAndReplace.% 5/15/96 dgp	cosmetic changes. Replaced RemoveWhite by deblank; %				disp(sprintf(...)) by fprintf(...'\n').%				Deleted unused variables.%				Changed "Line 1:" to "0001:", etc.% Open the filefilename = deblank(filename);if nargin~=2 | ~isstr(filename)	error('Usage: FindStringInFile(filename,string)');endfile=fopen(filename,'r');if file == (-1)	error(['FindStringInFile: Couldn''t open file ' filename]);endfprintf('%s\n',filename);lineCtr=1;line=fgets(file);while(line ~= [ -1 ]) % read till eof	if(length(line)>=length(string))		matches=findstr(line,string);	else		matches=[];	end	if (~isempty(matches))		fprintf('%04g: %s',lineCtr,line);	end	line=fgets(file);	lineCtr = lineCtr+1;endfclose(file);