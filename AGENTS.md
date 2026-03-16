You are in the repository of offline-LightningATM-esp32 diy bitcoin lightning coin exchange atm project. A diy machine where users can insert metal coins and will get shown a lnurl-withdraw qr code to withdraw the inserted amount in bitcoin to their wallet.
The software is running on an esp32 dev board or a waveshare esp32 dev board, depending on the setup. Different sizes of waveshare epaper displays are supported. You can get an overview about the project in the README.md file.
The project was originally implemented in C++/Arduino and is currently in the process of being rewritten in the Rust programming language. The old, stable C++/Arduino source code is in src/main.cpp and its header files are in includes/*. The new Rust codebase is being developed in the offline-lightning-atm-esp32 subdirectory. The offline-lightning-atm-esp32/dev.md file contains development notes for the rust toolchain.
The Rust code style is supposed to be easy to review and maintain and priorities simplicity over performance. Care also has to be taken to correctly interface with the esp32 and periphery through good embedded programming practices. 
LNURL-withdrawal:
The specification can be found at https://github.com/lnurl/luds/blob/luds/03.md.

Always compile and run unittests to verify your code changes are working as intended.
