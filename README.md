# Optocom replacement PCB FW
This repo contains firmware for a STM32F103-based PCB meant to replace the control board on a Durst Optocom enlarger. Plain old C23.

## Libraries
Nanopb (Protobuf) is used for communication with the external control box.
SeggerRTT is used for debugging purposes.

## Building
```./scripts/setup_venv.sh
    source .venv/bin/activate.fish
    /usr/bin/cmake -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_TOOLCHAIN_FILE:FILEPATH=cmake/toolchain.cmake -Bbuild --no-warn-unused-cli -G Ninja
    ninja optograin.elf
```