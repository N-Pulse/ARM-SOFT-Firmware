init
reset halt
bp 0x080056bc 2 hw
resume
sleep 1500
halt
echo "=== Registers at HardFault entry ==="
arm semihosting disable
mdw 0xE000ED28 1
mdw 0xE000ED2C 1
mdw 0xE000ED38 1
echo "=== read MSP and PSP via memory map ==="
set msp [reg msp -force]
set psp [reg psp -force]
echo "MSP = $msp"
echo "PSP = $psp"
echo "=== dump 32 words at PSP (likely task context exception frame) ==="
mdw $psp 32
echo "=== dump 32 words at MSP ==="
mdw $msp 32
exit
