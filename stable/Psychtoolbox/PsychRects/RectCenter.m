function [x, y] = RectCenter(r);
%   [x,y] = RectCenter(rect);
%
%	RectCenter returns the integer x,y point closest to the center of a rect.  
%
%	See also PsychRects, CenterRectOnPoint.

%	9/13/99	Allen Ingling wrote it.
%	10/6/99	dgp Fixed bug.

if nargout~=2
	error('Usage: [x, y] = RectCenter(rect);');
end
x = round(0.5*(r(1)+r(3)));
y = round(0.5*(r(2)+r(4)));
