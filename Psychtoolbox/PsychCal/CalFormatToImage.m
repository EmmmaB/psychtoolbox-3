function image = CalFormatToImage(calFormat,n,m)
%
% 
% Convert a calibration format image back to a real
% image.
%
% 8/04/04	dhb		Wrote it.

k = size(calFormat,1);
image = reshape(calFormat',m,n,k);