function secs=BusTicksToSecs(ticks)% secs = BusTicksToSecs(ticks)%% OS X or 9: ______________________________________________________________%% Converts bus ticks to seconds. %% WINDOWS: ________________________________________________________________% % BusTicksToSecs does not exist in Windows.% % _________________________________________________________________________%% See also: GetBusTicksTick, GetBusTicks%   HISTORY:%   04/18/03 awi Wrote BusTicksToSecs.m%   01/29/05 dgp Enhanced to support OS9.if IsOSX | IsOS9    secs=ticks*GetBusTicksTick;else    error(['BusTicksToSecs function not implemented on platform ' computer ]);end