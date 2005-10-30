function ShowTiff(whichScreen,rowPixels,colPixels,GAMMA)% ShowTiff(whichScreen,[rowPixels,colPixels],[GAMMA])%% Shows a TIFF image centered on the screen.%% If GAMMA is 1, gamma correction for current screen% is done.  Otherwise, a linear CLUT is used.%% 4/13/98  dhb  Wrote it.% Define screenif (nargin < 4 | isempty(GAMMA))	GAMMA = 0;endif (nargin < 3 | isempty(colPixels) | isempty(rowPixels))	colPixels = [];	rowPixels = [];endif (nargin == 2)	error('Usage: ShowTiff([whichScreen],[rowPixels,colPixels],[GAMMA]');endif (nargin < 1 | isempty(whichScreen))	whichScreen=0;end% Get name of TIFF file, assumed in current directorytiffName = [];while (isempty(tiffName))	tiffName = input('Enter full TIFF image filename: ','s');endimageData = imread(tiffName,'tif');% Show as grayscale or color, depending on image mode.if (ndims(imageData) == 2)	[window,screenRect]=Screen(whichScreen,'OpenWindow',0,[],8);else	if isempty(find(Screen(whichScreen,'PixelSizes') == 32))		fprintf('Sorry, this screen does not support full color operations\n');		return;	end	[window,screenRect]=Screen(whichScreen,'OpenWindow',0,[],32);end% Gamma correct if desiredif (GAMMA)	cal = LoadCalFile(whichScreen);	cal = SetGammaMethod(cal,1);	gammaDevice = linspace(0,1,256);	gammaDevice = gammaDevice(ones(3,1),:);	gammaClut = DeviceToSettings(cal,gammaDevice);	Screen(window,'SetClut',gammaClut');else	Screen(window,'SetClut',(0:255)'*ones(1,3));end% Make up the rectif (~isempty(colPixels))	theRect = CenterRect([0 0 colPixels rowPixels],screenRect);else	minDim = min([screenRect(3) screenRect(4)]);	theRect =  CenterRect([0 0 minDim minDim],screenRect);end% Write in the imageScreen(window,'PutImage',imageData,theRect);HideCursor;% Wait for userAsk(window,'Click when done.',255,0);% QuitShowCursor;Screen('CloseAll');