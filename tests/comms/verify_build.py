"""UART4 vector, single handle/RX owner, RTOS wait and default invariants.

Inspect real ARM ELF/objects; does not prove measured latency or electrical RX.
"""
from pathlib import Path
import re
import struct
import subprocess
ROOT = Path(__file__).resolve().parents[2]

def output(*args):
    return subprocess.check_output(args, cwd=ROOT, text=True)

def calls(target, function, expected=(), forbidden=()):
    assembly = output('arm-none-eabi-objdump', '-dr', f'--disassemble={function}', str(target))
    assert f'<{function}>' in assembly, (function, 'missing')
    for name in expected:
        assert f'<{name}>' in assembly or re.search(r'R_ARM_THM_\S+\s+'+re.escape(name)+r'\b', assembly), (function, name)
    for name in forbidden: assert name not in assembly, (function, name)

def verify(mode):
    directory = ROOT / 'build' / mode
    elf = directory / 'n-pulse.elf'
    symbols = {}
    for line in output('arm-none-eabi-nm', '-n', str(elf)).splitlines():
        match = re.match(r'^([0-9a-fA-F]+)\s+(\S)\s+(.+)$', line)
        if match: symbols[match[3]] = (int(match[1],16),match[2])
    for name in ('UART4_IRQHandler', 'HAL_UART_ErrorCallback', 'HAL_UART_TxCpltCallback'):
        assert symbols[name][1] == 'T', (mode, name)
    assert symbols['huart4'][1] == 'B' and 'hcom_uart' not in symbols
    assert symbols['LPUART1_IRQHandler'][1] == 'W'
    assert 'g_rxring' not in symbols and 'handle_rx_byte_external' not in symbols
    assert 'hdma_uart4_rx' not in symbols
    assert 'HAL_UART_Transmit' not in symbols and 'HAL_UART_Receive' not in symbols
    vector_file = ROOT / 'build/validation/lot3' / f'{mode}-vectors.bin'
    subprocess.run(['arm-none-eabi-objcopy','-O','binary','--only-section=.isr_vector',str(elf),str(vector_file)],check=True)
    values = struct.unpack(f'<{vector_file.stat().st_size//4}I', vector_file.read_bytes())
    device = (ROOT/'Drivers/CMSIS/Device/ST/STM32G4xx/Include/stm32g474xx.h').read_text()
    irq = int(re.search(r'UART4_IRQn\s*=\s*(\d+)',device)[1])
    assert values[16+irq] == symbols['UART4_IRQHandler'][0] | 1
    lpuart = int(re.search(r'LPUART1_IRQn\s*=\s*(\d+)',device)[1])
    assert values[16+lpuart] == symbols['Default_Handler'][0] | 1
    calls(elf, 'UART4_IRQHandler', ('HAL_UART_IRQHandler',))
    calls(elf, 'BoardUART_GetHandle', (), ('hcom_uart',))
    getter = output('arm-none-eabi-objdump','-dr','--disassemble=BoardUART_GetHandle',
                    str(directory/'obj/fw/comm/board_uart_compat.o'))
    assert re.search(r'R_ARM_ABS32\s+huart4\b',getter)
    calls(elf, 'UART4Transport_TxOnce', ('HAL_UART_Transmit_IT','ulTaskGenericNotifyTake','HAL_UART_AbortTransmit'), ('HAL_UART_Transmit>',))
    calls(elf, 'MotorSafety_RequestStopCauseLocked', ('CommsSession_OnStopLocked',), ('HAL_UART_', 'xQueue'))
    calls(elf, 'MotorSafety_Poll', ('CommsSession_PollLocked',))
    transport = directory/'obj/fw/comm/uart4_transport.o'
    session = directory/'obj/fw/comm/comms_session.o'
    calls(session,'CommsSession_PollLocked',('MotorSafety_RequestStopCauseLocked',),('xQueue','FromISR'))
    if mode == 'HW':
        assert symbols['HAL_UART_RxCpltCallback'][1] == 'T'
        calls(elf, 'Comms_ProcessRxOnce', ('ulTaskGenericNotifyTake','CommsParser_Feed'), ('HAL_UART_Receive>',))
        calls(elf, 'HAL_UART_RxCpltCallback', ('HAL_UART_Receive_IT','notify_rx'), ('HandleDeviceMessage','CommsParser_Feed'))
        calls(elf, 'notify_rx', ('vTaskGenericNotifyGiveFromISR',))
        calls(elf,'Motor_SetTargetAtEpoch',('MotorSafety_EpochLocked','MotorSafety_CanArmLocked','Motor_SetTarget'))
    else:
        calls(elf,'UART4Transport_StartRx',('HAL_UART_Receive_IT',))
        calls(elf,'HAL_UART_RxCpltCallback',('HAL_UART_Receive_IT','notify_rx'),('SimParser','HandleDeviceMessage'))
        calls(elf,'Comms_ProcessRxOnce',('ulTaskGenericNotifyTake','SimParser_Feed','SimSession_Service'),('CommsParser_Feed','HandleDeviceMessage'))
        calls(elf,'frame_received',('SimSession_OnFrame',),('HandleDeviceMessage',))
        for name in ('CommsParser_Feed','HandleDeviceMessage','process_joint_feedback','process_fsr_feedback'):
            assert name not in symbols, (mode,'unexpected receiver',name)
        calls(elf,'SimSession_Service',('Sim_Encode','UART4Transport_QueueSimFrame'),('HAL_UART_',))
        calls(elf,'UART4Transport_TxOnce',('take_sim_frame','HAL_GetTick'))
        calls(elf,'MotorSafety_Poll',('SimSession_PollLocked',))
    print(f'{mode}: UART4 vector/strong callbacks, single huart4, whole-frame IT TX, LINK interlock, no LPUART1 consumer OK')

def source_checks():
    msp = (ROOT/'Core/Src/stm32g4xx_hal_msp.c').read_text()
    ioc = (ROOT/'N-PULSE FIRMWARE.ioc').read_text()
    cfg = (ROOT/'fw/comm/comms_config.h').read_text()
    assert 'HAL_NVIC_SetPriority(UART4_IRQn, COMMS_UART_IRQ_PRIORITY, 0U)' in msp
    assert re.search(r'#define COMMS_UART_IRQ_PRIORITY 6U',cfg)
    assert r'NVIC.UART4_IRQn=true\:6\:0' in ioc
    assert 'Dma.UART4_RX' not in ioc and 'NVIC.DMA1_Channel6_IRQn=' not in ioc
    assert 'PC10.Signal=UART4_TX' in ioc and 'PC11.Signal=UART4_RX' in ioc
    rtoscfg = (ROOT/'Core/Inc/FreeRTOSConfig.h').read_text()
    assert re.search(r'configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY\s+5',rtoscfg)
    main = (ROOT/'Core/Src/main.c').read_text()
    assert 'huart4.Init.BaudRate = 115200' in main
    comm = (ROOT/'fw/comm/comm.c').read_text()
    assert 'tskIDLE_PRIORITY + 2U' in comm and 'ulTaskNotifyTake(pdTRUE, wait)' in comm
    assert 'xTimer' not in comm and 'FromISR' not in comm
    safety = (ROOT/'fw/motors/motor_control/motor_safety_config.h').read_text()
    assert '#define MOTOR_CONTROL_MAX_ELAPSED_MS 50U' in safety
    print('Sources: UART4 PC10/PC11 115200, priority 6 >= RTOS ceiling 5, RX task below control, timer watchdog removed, control maximum unchanged')

if __name__ == '__main__':
    source_checks()
    verify('HW'); verify('SIM')
