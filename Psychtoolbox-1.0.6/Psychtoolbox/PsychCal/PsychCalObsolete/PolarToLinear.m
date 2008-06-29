function [linear] = PolarToLinear(cData,pol)
% [linear] = PolarToLinear(cData,pol)
%
% Converts from linear polar coordinates to linear
% rectangular coordinates.
%
% Polar coordinates are defined as radius, azimuth, and elevation.
%
% PolarToLinear has been renamed "PolarToSensor".  The old
% name, "PolarToLinear", is still provided for compatability 
% with existing scripts but will disappear from future releases 
% of the Psychtoolbox.  Please use PolarToSensor instead.
%
% See Also: PsychCal, PsychCalObsolete, PolarToSensor

% 9/26/93    dhb   Added calData argument.
% 2/6/94     jms   Changed 'polar' to 'pol'
% 2/20/94    jms   Added single argument case to avoid cData.
% 4/5/02     dhb, ly  Call through new interface.
% 4/11/02    awi   Added help comment to use PolarToSensor instead.
%                  Added "See Also"
% 4/25/02    dhb   Fixed typo introduced in conversion.


if (nargin==1)
  linear = PolarToSensor(cData);
else
  linear = PolarToSensor(cData,pol);
end
