@echo off
set GIN_MODE=release
go build -o bin/svcctl.exe svcctl
call installer/build.cmd 1.0.2
cd ..