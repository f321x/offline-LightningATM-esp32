### some notes so i don't forget these things

```
$ cargo install espup --locked
$ espup install
$ cat $HOME/export-esp.sh >> ~/.bashrc
$ cargo install cargo-generate espflash ldproxy
$ cargo generate esp-rs/esp-idf-template cargo
$ cargo build
$ cargo run
$ espflash flash target/xtensa-esp32-espidf/debug/your-project-name
$ espflash monitor
(or)
$ cargo install cargo-espflash
$ cargo espflash flash --monitor
```

Unittests 
```
$ cargo test --lib --no-default-features --target x86_64-unknown-linux-gnu
```