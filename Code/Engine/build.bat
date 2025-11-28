@echo off
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /t:Build /m /nologo /v:minimal
