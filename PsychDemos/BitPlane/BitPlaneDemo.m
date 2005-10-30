% BitPlaneDemo%% Demo program to show how to store binary images% in each of 8 bit planes.  Here we just show% some square waves of different spatial frequencies,% but the image in each plane can be arbitrary.% 4/8/97  dhb  Wrote it in response to question from Lenny Kontsevich.% 3/12/98   dgp   Use Ask.% 4/3/99   dgp   Force 8-bit mode, since the program assumes it.% 4/08/02  awi  Added conditionals for Windows.% Generate eight binary squarewave images.% Scale each to the appropriate bit plane.% (The scale factor is simply 2^(i-1), where% i = 1,...,8.)imSize = 128;im1 = (2^(1-1))*MakeSquareBP(1,imSize);im2 = (2^(2-1))*MakeSquareBP(2,imSize);im3 = (2^(3-1))*MakeSquareBP(3,imSize);im4 = (2^(4-1))*MakeSquareBP(4,imSize);im5 = (2^(5-1))*MakeSquareBP(5,imSize);im6 = (2^(6-1))*MakeSquareBP(6,imSize);im7 = (2^(7-1))*MakeSquareBP(7,imSize);im8 = (2^(8-1))*MakeSquareBP(8,imSize);% Set up some lookup tables.  The interesting% ones pick out individual% bit planes.  Note that the colors we use for% the two binary states don't have to be the% same for every image, although here they% happen to be.zeroColor = [0 0 0];oneColor = [255 255 255];offClut = zeros(256,3);grayClut = (0:255)'*ones(1,3);clut1 = MakeBitClutBP(1,zeroColor,oneColor);clut2 = MakeBitClutBP(2,zeroColor,oneColor);clut3 = MakeBitClutBP(3,zeroColor,oneColor);clut4 = MakeBitClutBP(4,zeroColor,oneColor);clut5 = MakeBitClutBP(5,zeroColor,oneColor);clut6 = MakeBitClutBP(6,zeroColor,oneColor);clut7 = MakeBitClutBP(7,zeroColor,oneColor);clut8 = MakeBitClutBP(8,zeroColor,oneColor);% Open a window and set the clut so that nothing shows.whichScreen = 0;[window,winRect] = Screen(whichScreen,'OpenWindow',0,[],8);Screen(window,'SetClut',offClut);HideCursor;imRect = CenterRect(SetRect(0,0,imSize,imSize),winRect);% Screen 'PutImage' copy mode options exist only OS9.if IsOS9	% OR in the binary images, using the copy mode feature of	% Screen('PutImage',....);	Screen(window,'PutImage',im1,imRect,'srcOr');	Screen(window,'PutImage',im2,imRect,'srcOr');	Screen(window,'PutImage',im3,imRect,'srcOr');	Screen(window,'PutImage',im4,imRect,'srcOr');	Screen(window,'PutImage',im5,imRect,'srcOr');	Screen(window,'PutImage',im6,imRect,'srcOr');	Screen(window,'PutImage',im7,imRect,'srcOr');	Screen(window,'PutImage',im8,imRect,'srcOr');else	% on non-mac platforms we combine the image planes before copying to the screen.    orMat = im1+im2+im3+im4+im5+im6+im7+im8;    Screen(window,'PutImage',orMat, imRect);end        % Use clut to show the eight bit planesScreen(window,'SetClut',clut1);Ask(window,'First plane: click to show second',255,0);Screen(window,'SetClut',clut2);Ask(window,'Click to show third plane',255,0);Screen(window,'SetClut',clut3);Ask(window,'Click to show fourth plane',255,0);Screen(window,'SetClut',clut4);Ask(window,'Click to show fifth plane',255,0);Screen(window,'SetClut',clut5);Ask(window,'Click to show sixth plane',255,0);Screen(window,'SetClut',clut6);Ask(window,'Click to show seventh plane',255,0);Screen(window,'SetClut',clut7);Ask(window,'Click to show eigth plane',255,0);Screen(window,'SetClut',clut8);% Demonstrate how we can re-write just one planeclear3 = (2^(3-1))*ones(imSize,imSize);Ask(window,'Click to show third plane',255,0);Screen(window,'SetClut',clut3);Ask(window,'Click to clear third plane only',255,0);if IsOS9    Screen(window,'PutImage',clear3,imRect,'srcBic');else    orMat = orMat - im3;    Screen(window,'PutImage',orMat,imRect);end Ask(window,'Click to show second plane (not cleared)',255,0);Screen(window,'SetClut',clut2);Ask(window,'Click to show fourth plane (not cleared)',255,0);Screen(window,'SetClut',clut4);Ask(window,'Click to show third plane again (cleared)',255,0);Screen(window,'SetClut',clut3);Ask(window,'Click to re-write third plane',255,0);if IsOS9    Screen(window,'PutImage',im3,imRect,'srcOr');else    orMat = orMat + im3;    Screen(window,'PutImage',orMat,imRect);end % Show what the frame buffer looks like when we put% in a standard linear ramp clut.Ask(window,'Click to see frame buffer for linear clut',255,0);Screen(window,'SetClut',grayClut);% Close the windowAsk(window','Click to exit',255,0);ShowCursor;Screen('CloseAll');