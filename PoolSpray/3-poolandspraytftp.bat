echo reset | nc -w 1 -u 192.168.10.250 2301
tftp -i 192.168.10.250 PUT c:\Windows\Temp\arduino_build\sketch.bin
