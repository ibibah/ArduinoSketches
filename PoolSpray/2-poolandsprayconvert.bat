@echo off
echo "conversion de C:\Windows\Temp\arduino_build\PoolSpray.ino.elf en c:\Windows\Temp\arduino_build\sketch.bin"
"C:\Program Files (x86)\Arduino\hardware\tools\avr\bin\avr-objcopy.exe" -I ihex "C:\Windows\Temp\arduino_build\PoolSpray.ino.elf" -O binary c:\Windows\Temp\arduino_build\sketch.bin
rem echo "copie du fichier c:\Windows\Temp\arduino_build\sketch.bin vers wal@ibibahjeedom:~/poolandspray/sketch.bin"
rem pause
rem scp c:\Windows\Temp\arduino_build\sketch.bin wal@ibibahjeedom:~/poolandspray/sketch.bin
