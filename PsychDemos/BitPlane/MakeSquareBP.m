function image = MakeSquareBP(freq,imSize)% image = MakeSquareBP(freq,size)%% Make a one-d square wave image.% This is a quick and dirty routine% for the demo program.%% 4/8/97  dhb  Wrote it.image = sign(MakeCosImage(freq,0,imSize));index = find(image < 0);image(index) = zeros(size(index));