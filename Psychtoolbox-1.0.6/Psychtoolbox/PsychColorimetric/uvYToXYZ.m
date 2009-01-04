function XYZ = uvYToXYZ(uvY)
% XYZ = uvYToXYZ(uvY)
%
% Compute tristimulus coordinates from
% chromaticity and luminance.
%
% 10/31/94	dhb		Wrote it

[m,n] = size(uvY);
XYZ = zeros(m,n);
for i = 1:n
  XYZ(1,i) = (9/4)*uvY(3,i)*uvY(1,i)/uvY(2,i);
  XYZ(2,i) = uvY(3,i);
	denom = 9*uvY(3,i)/uvY(2,i);
  XYZ(3,i) = (denom - XYZ(1,i)-15*XYZ(2,i))/3;
end