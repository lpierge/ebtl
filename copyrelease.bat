@echo off
if not exist C:\ExplorerBgToolRe\NUL goto error
copy .\Release\ebtl.exe C:\ExplorerBgToolRe
goto done
:error
error: C:\ExplorerBgToolRe does not exist
goto done
:done