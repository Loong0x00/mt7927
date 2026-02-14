@echo off
%systemdrive%
cd %windir%/system32/
echo Please wait while installing drivers. Do not turn off or unplug the computer power during the installation...

@echo pnputil -a "mtkwecx.inf" /install
pnputil -a "%~dp0mtkwecx.inf" /install



@echo Done

EXIT /B
