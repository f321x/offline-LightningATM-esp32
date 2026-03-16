mod lnurl;
mod config;
mod util;
mod display;
mod coins;
mod state;
mod board;

use esp_idf_hal::{
    delay::Delay,
    gpio::{PinDriver, Pull},
    peripherals::Peripherals,
    spi::*,
    prelude::FromValueType,
};
use esp_idf_hal::spi::config::MODE_0;
use esp_idf_hal::gpio::AnyIOPin;
use epd_waveshare::epd1in54_v2::Epd1in54;
use epd_waveshare::epd2in7b::Epd2in7b;
use epd_waveshare::epd2in13_v2::Epd2in13;
use epd_waveshare::prelude::*;

use std::thread;
use std::time::{Duration, Instant};

use crate::display::AtmDisplay;
use crate::state::AppState;
use crate::board::{BoardType, BoardPins};

fn main() {
    esp_idf_svc::sys::link_patches();
    esp_idf_svc::log::EspLogger::initialize_default();
    log::set_max_level(log::LevelFilter::Debug);

    let config = config::Config::open().expect("failed to get non-volatile storage");

    // Get LNBits connection details (mock for now if missing)
    let base_url = "https://lnbits.com/api/atm/v2/";
    let atm_secret = "jawdlkjawlkdjalwkjd"; 

    let mut peripherals = Peripherals::take().unwrap();
    
    // Determine board type from config or default
    let board_type_str = config.get_board_type().unwrap_or("Generic".to_string());
    let board_type = BoardType::from_str(&board_type_str);
    log::info!("Selected Board: {:?}", board_type);
    
    let pins = BoardPins::new(board_type, &mut peripherals).expect("Failed to initialize board pins");

    // --- GPIO Setup ---
    let mut coin_pin = PinDriver::input(pins.coin).unwrap();
    coin_pin.set_pull(Pull::Up).unwrap();
    
    let mut mosfet_pin = PinDriver::output(pins.mosfet).unwrap();
    mosfet_pin.set_low().unwrap(); // Start enabled (LOW = Accept)

    let mut button_pin = PinDriver::input(pins.button).unwrap();
    button_pin.set_pull(Pull::Up).unwrap();

    let mut button_led = PinDriver::output(pins.button_led).unwrap();
    button_led.set_high().unwrap(); // LED ON

    // --- Display Setup ---
    // E-paper specific pins
    let dc = PinDriver::output(pins.dc).unwrap();
    let rst = PinDriver::output(pins.rst).unwrap();
    let busy = PinDriver::input(pins.busy).unwrap();

    let spi_config = SpiConfig::new()
        .baudrate(4_u32.MHz().into())
        .data_mode(MODE_0);

    let mut spi = SpiDeviceDriver::new_single(
        peripherals.spi2,
        pins.sclk,
        pins.mosi,
        None::<AnyIOPin>,
        Some(pins.cs),
        &SpiDriverConfig::new(),
        &spi_config,
    ).unwrap();

    let mut delay = Delay::new_default();
    
    let display_type = config.get_display_type().unwrap_or("GxEPD2_150_BN".to_string());
    log::info!("Selected Display: {}", display_type);

    let mut display: Box<dyn AtmDisplay<SpiDeviceDriver<'static, SpiDriver>, Delay>> = match display_type.as_str() {
        "GxEPD2_270" | "GxEPD2_270_GDEY027T91" => {
            let epd = Epd2in7b::new(&mut spi, busy, dc, rst, &mut delay, None).unwrap();
            Box::new(display::Display2in7Wrapper { epd })
        },
        "GxEPD2_213_B74" | "GxEPD2_213_flex" => {
             let epd = Epd2in13::new(&mut spi, busy, dc, rst, &mut delay, None).unwrap();
             Box::new(display::Display2in13Wrapper { epd })
        },
        _ => {
             // Default to 1.54
            let epd = Epd1in54::new(&mut spi, busy, dc, rst, &mut delay, None).unwrap();
            Box::new(display::Display1in54Wrapper { epd })
        }
    };

    let mut coin_detector = coins::CoinDetector::new(coin_pin, mosfet_pin);
    let mut app_state = AppState::Idle;
    
    // Track accumulated amount
    let mut current_amount_cents: u64 = 0;

    loop {
        match app_state {
            AppState::Idle => {
                log::info!("State: Idle");
                button_led.set_high().unwrap(); // LED ON
                display.home_screen(&mut spi, &mut delay).unwrap();
                
                // Wait for coins or button
                loop {
                    match coin_detector.wait_for_event(|| button_pin.is_low()) {
                        coins::CoinInteraction::Button => {
                            log::info!("Button pressed in Idle");
                            thread::sleep(Duration::from_millis(500)); 
                            // TODO: Implement Config / Clean Screen
                            break; 
                        },
                        coins::CoinInteraction::Coin(val) => {
                            current_amount_cents = val;
                            app_state = AppState::CountingCoins(current_amount_cents);
                            break;
                        },
                        _ => {}
                    }
                    thread::sleep(Duration::from_millis(10));
                }
            }
            AppState::CountingCoins(amount) => {
                log::info!("State: CountingCoins - {}", amount);
                button_led.set_low().unwrap(); // LED OFF indicating busy/wait?
                
                display.show_inserted_amount(&mut spi, &mut delay, &format!("{:.2} EUR", amount as f64 / 100.0)).unwrap();
                
                // Enable coin acceptance again to allow more coins
                coin_detector.set_accepting(true);
                
                // Loop to accept more coins until Button is pressed
                loop {
                    match coin_detector.wait_for_event(|| button_pin.is_low()) {
                        coins::CoinInteraction::Button => {
                             log::info!("Button pressed - Finishing deposit");
                             app_state = AppState::WithdrawReady(current_amount_cents, String::new());
                             break;
                        },
                        coins::CoinInteraction::Coin(val) => {
                            current_amount_cents += val;
                            log::info!("Added {}, Total: {}", val, current_amount_cents);
                            display.show_inserted_amount(&mut spi, &mut delay, &format!("{:.2} EUR", current_amount_cents as f64 / 100.0)).unwrap();
                        },
                        _ => {}
                    }
                }
            }
            AppState::WithdrawReady(amount, _) => {
                log::info!("State: WithdrawReady - {}", amount);
                // Generate LNURL
                let lnurl_str = lnurl::make_lnurl(base_url, atm_secret, amount);
                
                display.show_qr(&mut spi, &mut delay, &lnurl_str).unwrap();
                
                // Wait for button to finish/reset
                button_led.set_high().unwrap(); // LED ON
                
                let start_time = Instant::now();
                loop {
                    if button_pin.is_low() {
                         log::info!("Button pressed - Resetting");
                         current_amount_cents = 0;
                         app_state = AppState::Idle;
                         break;
                    }
                    
                    if start_time.elapsed().as_secs() > 600 {
                        // Timeout 10 mins
                        current_amount_cents = 0;
                        app_state = AppState::Idle;
                        break;
                    }
                    
                    // TODO: Flash LED
                    thread::sleep(Duration::from_millis(100));
                }
            }
            _ => {
                app_state = AppState::Idle;
            }
        }
    }
}