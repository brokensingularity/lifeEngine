@echo off
setlocal

cd ../Binaries/Win64/
HeliumGame-Win64-Debug.exe ImportMesh -src "%1" -dst "%2" -name "%3" %*