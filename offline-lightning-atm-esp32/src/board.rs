use esp_idf_hal::gpio::*;
use esp_idf_hal::peripherals::Peripherals;

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum BoardType {
    Generic,
    Waveshare,
}

impl BoardType {
    pub fn from_str(s: &str) -> Self {
        match s {
            "Waveshare" => BoardType::Waveshare,
            _ => BoardType::Generic,
        }
    }
}

pub struct BoardPins {
    pub coin: AnyIOPin,
    pub mosfet: AnyIOPin,
    pub button: AnyIOPin,
    pub button_led: AnyIOPin,
    // SPI Pins
    pub sclk: AnyIOPin,
    pub mosi: AnyIOPin,
    pub cs: AnyIOPin,
    // E-Ink Control
    pub dc: AnyIOPin,
    pub rst: AnyIOPin,
    pub busy: AnyIOPin,
}

impl BoardPins {
    pub fn new(board: BoardType, _pins: &mut Peripherals) -> Result<Self, String> {
        // Using unsafe to create AnyIOPin from IDs.
        // This avoids moving Peripherals and allows dynamic selection.
        
        match board {
            BoardType::Generic => Ok(BoardPins {
                coin: unsafe { AnyIOPin::new(17) },
                mosfet: unsafe { AnyIOPin::new(16) },
                button: unsafe { AnyIOPin::new(32) },
                button_led: unsafe { AnyIOPin::new(21) },
                sclk: unsafe { AnyIOPin::new(18) },
                mosi: unsafe { AnyIOPin::new(23) },
                cs: unsafe { AnyIOPin::new(26) },
                dc: unsafe { AnyIOPin::new(25) },
                rst: unsafe { AnyIOPin::new(33) },
                busy: unsafe { AnyIOPin::new(27) },
            }),
            BoardType::Waveshare => Ok(BoardPins {
                // Coin/Mosfet/Btn same as Generic for now
                coin: unsafe { AnyIOPin::new(17) },
                mosfet: unsafe { AnyIOPin::new(16) },
                button: unsafe { AnyIOPin::new(32) },
                button_led: unsafe { AnyIOPin::new(21) },
                // Waveshare SPI/Control
                sclk: unsafe { AnyIOPin::new(13) },
                mosi: unsafe { AnyIOPin::new(14) },
                cs: unsafe { AnyIOPin::new(15) },
                dc: unsafe { AnyIOPin::new(27) },
                rst: unsafe { AnyIOPin::new(26) },
                busy: unsafe { AnyIOPin::new(25) },
            }),
        }
    }
}
