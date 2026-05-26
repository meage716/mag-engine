@echo off

REM --- Ayarlar ---
set ProjectName=main
set TargetDrive=D:\%ProjectName%

set CompilerFlags=-nologo -Od -Oi -W4 -WX -wd4100 -wd4189 -Z7 -FC
set LinkerFlags=-incremental:no -opt:ref user32.lib gdi32.lib

IF NOT EXIST build mkdir build
pushd build

echo --- Derleme Basliyor ---
REM Proje ismiyle exe olustur
cl %CompilerFlags% ..\main.c ..\engine.c /link %LinkerFlags% /out:%ProjectName%.exe

if %errorlevel% neq 0 (
    echo [HATA] Derleme basarisiz! Oyun calistirilmiyor.
    popd
    pause
    exit /b %errorlevel%
)
popd
echo --- Derleme Tamamlandi ---


echo --- D: Dizini Hazirlaniyor ---
REM D: uzerinde klasor yoksa olustur
IF NOT EXIST "%TargetDrive%" mkdir "%TargetDrive%"

REM Sadece calisan exe'yi (minimum boyut) D: klasorune kopyala
REM .obj ve .pdb dosyalari build klasorunde kalir
copy /Y "build\%ProjectName%.exe" "%TargetDrive%\%ProjectName%.exe" > nul
echo %ProjectName%.exe '%TargetDrive%' icine kopyalandi.


echo --- Oyun Baslatiliyor ---
REM Kopyalanan hedefe git ve calistir
pushd "%TargetDrive%"
start %ProjectName%.exe
popd

echo --- Islem Bitti ---