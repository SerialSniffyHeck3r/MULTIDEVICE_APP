이 파일은 부트로더입니다.

이 기기의 MCU STM32F407VGT6의 0x8000 0000 영역에서부터 시작하며, Firmware Update / Emergency Recovery mode를 구현합니다.
F/W 파일이 없고 정상 부팅될경우 0x8002 0000으로 점프하여 App을 시작한다. 
