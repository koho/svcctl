@echo off
set SVCCTL_VERSION=1.0.1
set BUILDDIR=%~dp0
cd /d %BUILDDIR% || exit /b 1
if not exist %BUILDDIR%\build MD %BUILDDIR%\build
call VsDevCmd.bat
set SDK_PLATFORM=VS2017
cl /Zc:wchar_t /D "_UNICODE" /D "UNICODE" /nologo /O2 /W3 /GL /DNDEBUG /MD "-I%WIX%sdk\%SDK_PLATFORM%\inc" actions.cpp /Fo.\build\ /link /LTCG /NOLOGO "/LIBPATH:%WIX%sdk\%SDK_PLATFORM%\lib\x86" msi.lib wcautil.lib dutil.lib kernel32.lib user32.lib advapi32.lib Version.lib shell32.lib /DLL /DEF:"actions.def" /IMPLIB:.\build\actions.lib /OUT:.\build\actions.dll || exit /b 1
set WIX_CANDLE_FLAGS=-nologo -dSVCCTL_VERSION=%SVCCTL_VERSION%
set WIX_LIGHT_FLAGS=-nologo -spdb -ext "%WIX%bin\\WixUtilExtension.dll" -ext "%WIX%bin\\WixUIExtension.dll" -cultures:zh-CN
"%WIX%bin\candle" %WIX_CANDLE_FLAGS% -out build\ -arch x86 svcctl.wxs
"%WIX%bin\light" %WIX_LIGHT_FLAGS% -out "..\\bin\\svcctl-%SVCCTL_VERSION%.msi" "build\\svcctl.wixobj"