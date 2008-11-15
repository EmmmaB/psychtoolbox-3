function resultFlag = IsWin

% function resultFlag = IsWin
%
% OSX, OS9: Returns true if the operating system is Windows.  Shorthand for:
% streq(computer,'PCWIN')
%
% WIN: Does not yet exist in Windows.
%
% See also: IsOS9, IsOSX

resultFlag=streq(computer,'PCWIN');