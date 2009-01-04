function PlayDualMoviesDemoOSX(moviename)
%
% PlayDualMoviesDemoOSX(moviename)
%
% This demo accepts a pattern for a valid moviename, e.g.,
% moviename='*.mpg', then it plays all movies in the current working
% directory whose names match the provided pattern, e.g., the '*.mpg'
% pattern would play all MPEG files in the current directory.
%
% This demo uses automatic asynchronous playback for synchronized playback
% of video and sound. Each movie plays until end, then rewinds and plays
% again from the start. Pressing the Cursor-Up/Down key pauses/unpauses the
% movie and increases/decreases playback rate.
% The left- right arrow keys jump in 1 seconds steps. SPACE jumps to the
% next movie in the list. ESC ends the demo.
%
% This demo needs MacOS-X 10.3.9 or 10.4.x with Quicktime-7 installed!

% History:
% 10/30/05  mk  Wrote it.

if nargin < 1
    moviename = '*.mov'
end;

try
    % Child protection
    AssertOpenGL;
    background=[128, 128, 128];

    % Open onscreen window:
    screen=max(Screen('Screens'));
    [win, scr_rect] = Screen('OpenWindow', screen);

    % Retrieve duration of a single video refresh interval:
    ifi = Screen('GetFlipInterval', win);
    
    % Clear screen to background color:
    Screen('FillRect', win, background);
    
    % Initial display and sync to timestamp:
    vbl=Screen('Flip',win);
    iteration=0;    
    abortit=0
    
    % Return full list of movie files from directory+pattern:
    moviefiles=dir(moviename);
    
    % Endless loop, runs until ESC key pressed:
    while (abortit<2)
        iteration=iteration + 1;
        moviename=moviefiles(mod(iteration, size(moviefiles,1))+1).name;
        moviename2=moviefiles(mod(iteration+1, size(moviefiles,1))+1).name;
        
        % Open movie file and retrieve basic info about movie:
        [movie movieduration fps imgw imgh] = Screen('OpenMovie', win, moviename);        
        fprintf('Movie1: %s  : %f seconds duration, %f fps...\n', moviename, movieduration, fps);
        rect1=SetRect(1,1,imgw,imgh);
        % Open 2nd movie file and retrieve basic info about movie:
        [movie2 movieduration fps imgw imgh] = Screen('OpenMovie', win, moviename2);        
        fprintf('Movie2: %s  : %f seconds duration, %f fps...\n', moviename2, movieduration, fps);
        rect2=SetRect(1,1,imgw,imgh);
        rect2=AdjoinRect(rect2, rect1, RectRight);
        
        i=0;
    
        % Seek to start of movies (timeindex 0):
        Screen('SetMovieTimeIndex', movie, 0);
        Screen('SetMovieTimeIndex', movie2, 0);

        rate=1;
        
        % Start playback of movies. This will start
        % the realtime playback clock and playback of audio tracks, if any.
        % Play 'movie', at a playbackrate = 1, with endless loop=1 and
        % 1.0 == 100% audio volume.
        Screen('PlayMovie', movie, rate, 1, 1.0);
        Screen('PlayMovie', movie2, rate, 1, 1.0);
    
        t1 = GetSecs;
        
        % Infinite playback loop: Fetch video frames and display them...
        while(1)
            i=i+1;
            if (abs(rate)>0)
                % Return next frame in movie, in sync with current playback
                % time and sound.
                % tex either the texture handle or zero if no new frame is
                % ready yet.
                tex=0;
                tex2=0;
                tex = Screen('GetMovieImage', win, movie, 0);
                tex2 = Screen('GetMovieImage', win, movie2, 0);

                % Valid texture returned?
                if tex>0
                    % Draw the new texture immediately to screen:
                    Screen('DrawTexture', win, tex, [], rect1);
                    % Release texture:
                    Screen('Close', tex);
                end;

                % Valid 2nd texture returned?
                if tex2>0
                    % Draw the new texture immediately to screen:
                    Screen('DrawTexture', win, tex2, [], rect2);
                    % Release texture:
                    Screen('Close', tex2);
                end;

                % Update display if there is anything to update:
                if (tex>0 || tex2>0)
                    % We use clearmode=1, aka don't clear on flip. This is
                    % needed to avoid flicker...
                    vbl=Screen('Flip', win, 0, 1);
                end;
            end;
            
            % Check for abortion:
            abortit=0;
            [keyIsDown,secs,keyCode]=KbCheck;
            if (keyIsDown==1 & keyCode(KbName('ESCAPE')))
                % Set the abort-demo flag.
                abortit=2;
                break;
            end;
            
            if (keyIsDown==1 & keyCode(KbName('SPACE')))
                % Exit while-loop: This will load the next movie...
                break;
            end;
            
            if (keyIsDown==1 & keyCode(KbName('RightArrow')))
                % Advance movietime by one second:
                Screen('SetMovieTimeIndex', movie, Screen('GetMovieTimeIndex', movie) + 1);
                Screen('SetMovieTimeIndex', movie2, Screen('GetMovieTimeIndex', movie2) + 1);
            end;

            if (keyIsDown==1 & keyCode(KbName('LeftArrow')))
                % Rewind movietime by one second:
                Screen('SetMovieTimeIndex', movie, Screen('GetMovieTimeIndex', movie) - 1);
                Screen('SetMovieTimeIndex', movie2, Screen('GetMovieTimeIndex', movie2) - 1);
            end;

            if (keyIsDown==1 & keyCode(KbName('UpArrow')))
                % Increase playback rate by 1 unit.
                while KbCheck; WaitSecs(0.01); end;
                if (keyCode(KbName('RightShift')))
                    rate=rate+0.1;
                else
                    rate=round(rate+1);
                end;
                Screen('PlayMovie', movie, rate, 1, 1.0);
                Screen('PlayMovie', movie2, rate, 1, 1.0);
            end;

            if (keyIsDown==1 & keyCode(KbName('DownArrow')))
                % Decrease playback rate by 1 unit.
                while KbCheck; WaitSecs(0.01); end;
                if (keyCode(KbName('RightShift')))
                    rate=rate-0.1;
                else
                    rate=round(rate-1);
                end;
                Screen('PlayMovie', movie, rate, 1, 1.0);
                Screen('PlayMovie', movie2, rate, 1, 1.0);
            end;
        end;
    
        telapsed = GetSecs - t1
        finalcount=i

        Screen('Flip', win);
        while KbCheck; end;
        
        % Done. Stop playback:
        Screen('PlayMovie', movie, 0);
        Screen('PlayMovie', movie2, 0);

        % Close movie objects:
        Screen('CloseMovie', movie);
        Screen('CloseMovie', movie2);
    end;
    
    % Close screens.
    Screen('CloseAll');

    % Done.
    return;
catch
    % Error handling: Close all windows and movies, release all ressources.
    Screen('CloseAll');
end;