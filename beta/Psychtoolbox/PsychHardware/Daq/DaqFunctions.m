function DaqFunctions
% The Daq Toolbox functions.
% 
% * Analog input/output commands *
% DaqAIn                  Read analog in
% DaqAInScan              Scan analog channels
% DaqAInStop              Stop input scan
% DaqAInScanBegin         Begin sampling.
% DaqAInScanContinue      Continue sampling: transfer data from Mac OS to PsychHID.
% DaqAInScanEnd           End sampling: data are returned.
% DaqALoadQueue           Set channel gains
% DaqAOut                 Write analog out
% DaqAOutScan             Clocked analog out
% DaqAOutStop             Stop output scan
% 
% * Digital input/output commands *
% DaqDConfigPort          Configure digital port
% DaqDIn                  Read digital ports
% DaqDOut                 Write digital port
% 
% * Miscellaneous commands *
% DaqDeviceIndex          Get reference(s) to our device(s)
% DaqBlinkLED             Cause LED to blink
% DaqCInit                Initialize counter
% DaqCIn                  Read counter
% DaqGetAll               Retrieve all analog and digital input values
% DaqGetStatus            Retrieve device status
% DaqReset                Reset the device
% DaqSetCal               Set CAL output
% DaqSetSync              Configure sync
% DaqSetTrigger           Configure ext. trigger
% 
% * Memory commands *
% DaqMemRead              Read memory
% DaqMemWrite             Write memory
% DaqReadCode             Read program memory
% DaqPrepareDownload      Prepare for program memory download
% DaqWriteCode            Write program memory
% DaqWriteSerialNumber    Write a new serial number to device
% 
% See also Daq, TestDaq, DaqPins, DaqCalls, DaqCodes,
% DaqDeviceIndex, DaqDIn, DaqDOut, DaqAIn, DaqAOut, DaqAInScan,DaqAOutScan.
help DaqFunctions