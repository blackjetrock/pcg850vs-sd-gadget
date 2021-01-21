10 #include "p16f84a.inc"
20 __config 0x3ff1 ;CP_OFF & PWRT_ON & WDT_OFF & XT_OSC
30DELAY1 equ 0x08 ;delay counter 1
40DELAY2 equ 0x09 ;delay counter 2
50 org 0
100start
110 bsf STATUS,RP0 ;change to bank 1
120 bcf TRISA,0 ;enable RA1 for output
130 bcf STATUS,RP0 ;back to bank 0
140loop
150 bsf PORTA,0 ;RA0=1,LED=on
160 call delay
170 bcf PORTA,0 ;RA0=0,LED=off
180 call delay
190 goto loop
300delay
310 movlw 255
320 movwf DELAY1
330 movwf DELAY2
340dloop
350 decfsz DELAY1,f
360 goto dloop
370 decfsz DELAY2,f
380 goto dloop
390 return
