% PutImageTest%% Transfer an RGB image from Matlab arrays to a Screen window, and back. % Tests all screens. Tests on-screen and off-screen windows.% 3/27/98  dhb  Wrote it, as ColorImageTest.% 3/31/98  dgp	Cosmetic changes; use the new BlackIndex and WhiteIndex.% 3/31/98  dgp	Test MxNx3 images as well. Test PutImage. % 3/31/98  dgp	Test pixelSize 16 and 32. % 3/31/98  dgp	More PutImage tests.% 3/31/98  dgp	Test double and uint8.% 3/31/98  dgp	Test more pixelSizes.% 4/6/98	 dgp	Use 'PixelSizes'.% 4/6/98	 dgp	Renamed: PutImageTest% 4/1/02   dgp	Test all screens. Test off-screen windows too. Remove all calls to PutColorImage.% 4/24/02 awi Exit on PC with message.if IsWin    error('Win: PutImageTest not yet supported.');end[c,maxElements]=computer;maxSize=floor(sqrt(maxElements));for whichScreen=Screen('Screens')	DescribeScreen(whichScreen);	fprintf('Testing: ...\n');	errors=0;	for pixelSize=Screen(whichScreen,'PixelSizes')		window=Screen(whichScreen,'OpenWindow',[],[],pixelSize);		offscreenWindow=Screen(whichScreen,'OpenOffscreenWindow');		for w=[window offscreenWindow]			fprintf('%2d bit pixel',pixelSize);			if w==window				fprintf(', on-screen window.\n');			else				fprintf(', off-screen window.\n');			end			black=BlackIndex(w);			white=WhiteIndex(w);						% Create a sinusoidal image in a Matlab matrix, in two steps. First			% create a sinusoidal vector, then replicate this vector to produce a			% sinusoidal image. The replication is done by an outer product. This is			% easy to read, though not the fastest way to do it.						nPixels = min(256,maxSize);			cyclesPerImage = 4;			vector = (1+sin(2*pi*cyclesPerImage*(1:nPixels)/nPixels))/2;			image1 = ones(nPixels,1)*(black+(white-black)*vector);			vector = (1-sin(2*pi*cyclesPerImage*(1:nPixels)/nPixels))/2;			image2 = ones(nPixels,1)*(black+(white-black)*vector);			r=round(image1);			g=round(image2);			b=zeros(size(image1));			clear image1 image2						% 'PutImage' accepts a color image as one MxNx3 array.						for isDouble=[1 0]				if isDouble					fprintf('	double array\n');					r = double(r);					g = double(g);					b = double(b);				else					fprintf('	uint8 array\n');					r = uint8(r);					g = uint8(g);					b = uint8(b);				end									fprintf('		luminance grating\n');				rect=[0 0 size(r,2) size(r,1)];				wRect=Screen(w,'Rect');				rect=CenterRect(rect,wRect);				Screen(w,'PutImage',r);				WaitSecs(0.5);				if pixelSize>8					rrr=cat(3,r,r,r);	% NxMx3 array, containing rgb image.				else					rrr=r;				end				rrrCopy=Screen(w,'GetImage',rect);				if ~all(double(rrr)==double(rrrCopy))					fprintf('			What GetImage read back differs from what PutImage wrote.\n');					disp('wrote:')					disp(squeeze(rrr(1,1:10,:))')					disp('read:')					disp(squeeze(rrrCopy(1,1:10,:))')					errors=errors+1;				end									if pixelSize>8					fprintf('		red-green grating\n');					rgb=cat(3,r,g,b);	% NxMx3 array, containing rgb image.					Screen(w,'PutImage',rgb);					rgbCopy=Screen(w,'GetImage',rect);					if ~all(rgb==rgbCopy)						fprintf('			What GetImage read back differs from what PutImage wrote.\n');						disp('wrote:')						disp(squeeze(rgb(1,1:10,:))')						disp('read:')						disp(squeeze(rgbCopy(1,1:10,:))')						errors=errors+1;					end							WaitSecs(0.5);				end			end		end		Screen(window,'Close');		Screen(offscreenWindow,'Close');	end	fprintf('Done. Every image was displayed by PutImage and read back by GetImage, with %d errors.\n', errors);endDescribeScreen(-1);