% MinimalExample.m% To: psychtoolbox@yahoogroups.com% From: Daniel Shima <daniel.shima@vanderbilt.edu>% Date: Thu, 27 Jun 2002 15:52:41 -0500% Subject: [psychtoolbox] minimal example for new users% % I do not know where one can find full tutorials for learning to use% Matlab or the Psychtoolbox, but here's a program% called MinimalExample.m which I have used to introduce people to the% basics of opening screens, drawing stuff (i.e., stimuli) to them,% displaying them for discrete amounts of time, and collecting keyboard% responses and response times.  Something upon which to build.% % Also see MovieDemo.% % And review the web tutorial, which among other things suggests typing% "help PsychBasic" and "help PsychDemos" in Matlab for an introduction to% core commands and routines.% web http://www.psychtoolbox.org/tutorial.html ;% % Hope that helps.% % web http://groups.yahoo.com/group/psychtoolbox/message/1151 ;  clear all;pixelSize = 8; % You might change to 32 if 8 is not working, depending on OS9 or Windows.[w, screenRect] = Screen(0,'OpenWindow',[],[],pixelSize); % Define your output screen as your monitor which is screen 0.black=BlackIndex(w); % Value depends on pixelSize.white=WhiteIndex(w); % "scr_one = Screen(w, 'OpenOffscreenWindow',black, screenRect); % Define scr_one, a black screen not yet visible.screen(scr_one,'FillOval',white,[10,10,90,90]); % Draw a white circle on scr_one.screen(scr_one,'DrawText','White circle, diameter 80 pixels.  Presented for 3 seconds.',10,150,white); % Draw text on scr_one.scr_two = Screen(w, 'OpenOffscreenWindow',black, screenRect); % Same for scr_two as scr_one.screen(scr_two,'FillRect',(black+white)/2,centerRect([0 0 100 50],screenRect)); % Draw rectangle to scr_two.screen(scr_two,'DrawText','Middle-gray rectangle, 100 x 50 pixels.  Press any key to continue.',50,200,(black+white)/2);scr_blank = Screen(w, 'OpenOffscreenWindow',black, screenRect); % scr_blank is just a black screen.% Scr_one for 3 seconds.Screen(w,'WaitBlanking'); % Wait for vertical sync signal before copying screen to monitor, not necessary.Screen('CopyWindow',scr_one,w); % Copy scr_one to monitor.starttime1 = GetSecs; % Record CPU time at this point.while GetSecs-starttime1 < 3 % Minimal while-loop that lets almost exactly 3 seconds pass.endfinishtime1 = GetSecs;% Scr_blank for 2 seconds.Screen(w,'WaitBlanking');Screen('CopyWindow',scr_blank,w);starttime2 = GetSecs;while GetSecs-starttime2 < 2endfinishtime2 = GetSecs;% Scr_two until subject presses a key.Screen(w,'WaitBlanking');Screen('CopyWindow',scr_two,w);starttime3 = GetSecs;while 1    [keyIsDown,secs,keyCode] = KbCheck; % In while-loop, rapidly and continuously check if any key being pressed.    if keyIsDown % If key is being pressed...        starttime4 = GetSecs; % ... record current CPU time...        break; % ... and end while-loop.	endendFlushEvents('keyDown','autoKey'); % discard the key press.% Scr_blank for 1 seconds.Screen(w,'WaitBlanking');Screen('CopyWindow',scr_blank,w);starttime5 = GetSecs;while GetSecs-starttime5 < 1endfinishtime5 = GetSecs;Screen('CloseAll'); % Close all screens, return to windows.fprintf('\nShould be 3 seconds of white circle: %5.3f\n',finishtime1-starttime1);fprintf('\nShould be 2 seconds of blank screen: %5.3f\n',finishtime2-starttime2);fprintf('\nSeconds it took you to press any key: %5.3f\n',starttime4-starttime3);fprintf('\nSeconds between key press and blank screen: %5.3f\n',starttime5-starttime4);fprintf('\nShould be 1 second of blank screen: %5.3f\n',finishtime5-starttime5);