use epd_waveshare::{
    epd1in54_v2::*,
    epd2in7b::*,
    epd2in13_v2::*,
    prelude::*,
};
use embedded_graphics::{
    mono_font::{ascii::*, MonoTextStyle},
    prelude::*,
    text::Text,
};

// Define a common trait for all displays
pub trait AtmDisplay<SPI, DELAY> 
where
    SPI: embedded_hal::spi::SpiDevice,
    DELAY: embedded_hal::delay::DelayNs,
{
    fn home_screen(&mut self, spi: &mut SPI, delay: &mut DELAY) -> Result<(), Box<dyn std::error::Error>>;
    fn show_inserted_amount(&mut self, spi: &mut SPI, delay: &mut DELAY, amount_string: &str) -> Result<(), Box<dyn std::error::Error>>;
    fn show_qr(&mut self, spi: &mut SPI, delay: &mut DELAY, qr_content: &str) -> Result<(), Box<dyn std::error::Error>>;
    #[allow(dead_code)]
    fn clean(&mut self, spi: &mut SPI, delay: &mut DELAY) -> Result<(), Box<dyn std::error::Error>>;
}

// --- Helper for drawing to reduce duplication ---
// We use a generic DrawTarget to support different Display buffers.
// However, resolution differs, so positioning is specific. 
// We will keep implementations separate for clarity on layout.

// ================= 1.54 Inch =================

pub struct Display1in54Wrapper<SPI, BUSY, DC, RST, DELAY> {
    pub epd: Epd1in54<SPI, BUSY, DC, RST, DELAY>,
}

impl<SPI, BUSY, DC, RST, DELAY> AtmDisplay<SPI, DELAY> for Display1in54Wrapper<SPI, BUSY, DC, RST, DELAY>
where
    SPI: embedded_hal::spi::SpiDevice,
    DELAY: embedded_hal::delay::DelayNs,
    BUSY: embedded_hal::digital::InputPin,
    DC: embedded_hal::digital::OutputPin,
    RST: embedded_hal::digital::OutputPin,
{
    fn home_screen(&mut self, spi: &mut SPI, delay: &mut DELAY) -> Result<(), Box<dyn std::error::Error>> {
        let mut display_buffer = Display1in54::default();
        display_buffer.set_rotation(DisplayRotation::Rotate90);
        display_buffer.clear(Color::White).map_err(|_| "Failed to clear buffer")?;

        let large_style = MonoTextStyle::new(&FONT_9X18_BOLD, Color::Black);
        let small_style = MonoTextStyle::new(&FONT_6X10, Color::Black);

        Text::new("Insert\nEuro coins\non the\nright\nside to\nstart ->", Point::new(0, 10), large_style)
            .draw(&mut display_buffer)
            .map_err(|_| "Failed to draw main text")?;

        Text::new("Prepare Lightning enabled Bitcoin\nwallet before starting!\n\nSupported coins:\n1 Cent and 2 Euro", Point::new(0, 160), small_style)
            .draw(&mut display_buffer)
            .map_err(|_| "Failed to draw instructions")?;

        self.epd.update_frame(spi, display_buffer.buffer(), delay).map_err(|_| "Failed to update frame")?;
        self.epd.display_frame(spi, delay).map_err(|_| "Failed to display frame")?;
        self.epd.sleep(spi, delay).map_err(|_| "Failed to sleep display")?;
        Ok(())
    }

    fn show_inserted_amount(&mut self, spi: &mut SPI, delay: &mut DELAY, amount_string: &str) -> Result<(), Box<dyn std::error::Error>> {
        let mut display_buffer = Display1in54::default();
        display_buffer.set_rotation(DisplayRotation::Rotate90);
        display_buffer.clear(Color::White).map_err(|_| "Failed to clear buffer")?;

        let large_style = MonoTextStyle::new(&FONT_9X18_BOLD, Color::Black);
        let huge_style = MonoTextStyle::new(&FONT_10X20, Color::Black); 

        Text::new("Inserted amount:", Point::new(0, 4), large_style).draw(&mut display_buffer)?;
        Text::new(amount_string, Point::new(10, 90), huge_style).draw(&mut display_buffer)?;
        Text::new(" Press button\n once finished.", Point::new(0, 160), large_style).draw(&mut display_buffer)?;

        self.epd.update_frame(spi, display_buffer.buffer(), delay).map_err(|_| "Failed to update frame")?;
        self.epd.display_frame(spi, delay).map_err(|_| "Failed to display frame")?;
        self.epd.sleep(spi, delay).map_err(|_| "Failed to sleep display")?;
        Ok(())
    }

    fn show_qr(&mut self, spi: &mut SPI, delay: &mut DELAY, qr_content: &str) -> Result<(), Box<dyn std::error::Error>> {
         let mut display_buffer = Display1in54::default();
        display_buffer.set_rotation(DisplayRotation::Rotate90);
        display_buffer.clear(Color::White).map_err(|_| "Failed to clear buffer")?;

        Text::new("QR Code Placeholder", Point::new(10, 50), MonoTextStyle::new(&FONT_6X10, Color::Black)).draw(&mut display_buffer)?;
        Text::new(qr_content, Point::new(10, 70), MonoTextStyle::new(&FONT_6X10, Color::Black)).draw(&mut display_buffer)?;

        self.epd.update_frame(spi, display_buffer.buffer(), delay).map_err(|_| "Failed to update frame")?;
        self.epd.display_frame(spi, delay).map_err(|_| "Failed to display frame")?;
        self.epd.sleep(spi, delay).map_err(|_| "Failed to sleep display")?;
        Ok(())
    }

    fn clean(&mut self, spi: &mut SPI, delay: &mut DELAY) -> Result<(), Box<dyn std::error::Error>> {
        self.epd.clear_frame(spi, delay).map_err(|_| "Failed to clear frame")?;
        self.epd.display_frame(spi, delay).map_err(|_| "Failed to display frame")?;
        self.epd.sleep(spi, delay).map_err(|_| "Failed to sleep display")?;
        Ok(())
    }
}

// ================= 2.7 Inch (Tri-Color B/W/R used as B/W) =================

pub struct Display2in7Wrapper<SPI, BUSY, DC, RST, DELAY> {
    pub epd: Epd2in7b<SPI, BUSY, DC, RST, DELAY>,
}

impl<SPI, BUSY, DC, RST, DELAY> AtmDisplay<SPI, DELAY> for Display2in7Wrapper<SPI, BUSY, DC, RST, DELAY>
where
    SPI: embedded_hal::spi::SpiDevice,
    DELAY: embedded_hal::delay::DelayNs,
    BUSY: embedded_hal::digital::InputPin,
    DC: embedded_hal::digital::OutputPin,
    RST: embedded_hal::digital::OutputPin,
{
    fn home_screen(&mut self, spi: &mut SPI, delay: &mut DELAY) -> Result<(), Box<dyn std::error::Error>> {
        // Epd2in7b resolution 176 x 264
        let mut display_buffer = Display2in7b::default();
        display_buffer.set_rotation(DisplayRotation::Rotate90);
        display_buffer.clear(Color::White).map_err(|_| "Failed to clear buffer")?;

        let large_style = MonoTextStyle::new(&FONT_10X20, Color::Black);
        let small_style = MonoTextStyle::new(&FONT_6X10, Color::Black);

        Text::new("Insert Euro coins\n on the right ->\n to start ATM", Point::new(11, 20), large_style)
            .draw(&mut display_buffer)?;

        Text::new("Prepare Lightning enabled Bitcoin\n  wallet before starting!\n  Supported coins: 5 - 50 Cent, 1 - 2 Euro", Point::new(12, 140), small_style)
            .draw(&mut display_buffer)?;

        // Epd2in7b requires two buffers: black and chromatic (red).
        let chromatic_buffer = [0xFFu8; 176 * 264 / 8]; // Size must match. 176*264/8 = 5808 bytes.
        
        self.epd.update_color_frame(spi, delay, display_buffer.buffer(), &chromatic_buffer).map_err(|_| "Failed to update color frame")?;
        self.epd.display_frame(spi, delay).map_err(|_| "Failed to display frame")?;
        self.epd.sleep(spi, delay).map_err(|_| "Failed to sleep display")?;
        Ok(())
    }

    fn show_inserted_amount(&mut self, spi: &mut SPI, delay: &mut DELAY, amount_string: &str) -> Result<(), Box<dyn std::error::Error>> {
        let mut display_buffer = Display2in7b::default();
        display_buffer.set_rotation(DisplayRotation::Rotate90);
        display_buffer.clear(Color::White).map_err(|_| "Failed to clear buffer")?;

        let large_style = MonoTextStyle::new(&FONT_10X20, Color::Black);
        let huge_style = MonoTextStyle::new(&FONT_10X20, Color::Black);

        Text::new("Inserted amount:", Point::new(11, 10), large_style).draw(&mut display_buffer)?;
        Text::new(amount_string, Point::new(20, 75), huge_style).draw(&mut display_buffer)?;
        Text::new("Press button\n once finished.", Point::new(11, 135), large_style).draw(&mut display_buffer)?;

        let chromatic_buffer = [0xFFu8; 5808];
        self.epd.update_color_frame(spi, delay, display_buffer.buffer(), &chromatic_buffer).map_err(|_| "Failed to update color frame")?;
        self.epd.display_frame(spi, delay).map_err(|_| "Failed to display frame")?;
        self.epd.sleep(spi, delay).map_err(|_| "Failed to sleep display")?;
        Ok(())
    }

    fn show_qr(&mut self, spi: &mut SPI, delay: &mut DELAY, qr_content: &str) -> Result<(), Box<dyn std::error::Error>> {
        let mut display_buffer = Display2in7b::default();
        display_buffer.set_rotation(DisplayRotation::Rotate90);
        display_buffer.clear(Color::White).map_err(|_| "Failed to clear buffer")?;

        Text::new("Please scan QR code:", Point::new(11, 5), MonoTextStyle::new(&FONT_10X20, Color::Black)).draw(&mut display_buffer)?;
        Text::new("Reset - press button", Point::new(11, 155), MonoTextStyle::new(&FONT_10X20, Color::Black)).draw(&mut display_buffer)?;
        
        Text::new(qr_content, Point::new(80, 80), MonoTextStyle::new(&FONT_10X20, Color::Black)).draw(&mut display_buffer)?;

        let chromatic_buffer = [0xFFu8; 5808];
        self.epd.update_color_frame(spi, delay, display_buffer.buffer(), &chromatic_buffer).map_err(|_| "Failed to update color frame")?;
        self.epd.display_frame(spi, delay).map_err(|_| "Failed to display frame")?;
        self.epd.sleep(spi, delay).map_err(|_| "Failed to sleep display")?;
        Ok(())
    }

    fn clean(&mut self, spi: &mut SPI, delay: &mut DELAY) -> Result<(), Box<dyn std::error::Error>> {
        self.epd.clear_frame(spi, delay).map_err(|_| "Failed to clear frame")?;
        self.epd.display_frame(spi, delay).map_err(|_| "Failed to display frame")?;
        self.epd.sleep(spi, delay).map_err(|_| "Failed to sleep display")?;
        Ok(())
    }
}

// ================= 2.13 Inch (V2) =================

pub struct Display2in13Wrapper<SPI, BUSY, DC, RST, DELAY> {
    pub epd: Epd2in13<SPI, BUSY, DC, RST, DELAY>,
}

impl<SPI, BUSY, DC, RST, DELAY> AtmDisplay<SPI, DELAY> for Display2in13Wrapper<SPI, BUSY, DC, RST, DELAY>
where
    SPI: embedded_hal::spi::SpiDevice,
    DELAY: embedded_hal::delay::DelayNs,
    BUSY: embedded_hal::digital::InputPin,
    DC: embedded_hal::digital::OutputPin,
    RST: embedded_hal::digital::OutputPin,
{
    fn home_screen(&mut self, spi: &mut SPI, delay: &mut DELAY) -> Result<(), Box<dyn std::error::Error>> {
        // Epd2in13 V2 resolution is 122x250
        let mut display_buffer = Display2in13::default();
        display_buffer.set_rotation(DisplayRotation::Rotate90);
        display_buffer.clear(Color::White).map_err(|_| "Failed to clear buffer")?;

        let large_style = MonoTextStyle::new(&FONT_9X18_BOLD, Color::Black);
        let small_style = MonoTextStyle::new(&FONT_6X10, Color::Black);

        Text::new("LIGHTNING ATM", Point::new(5, 5), large_style).draw(&mut display_buffer)?;
        Text::new("Insert coins\non the right\nside to start", Point::new(3, 33), small_style).draw(&mut display_buffer)?;

        Text::new("Prepare Lightning enabled Bitcoin\nwallet before starting!", Point::new(0, 95), small_style).draw(&mut display_buffer)?;

        self.epd.update_frame(spi, display_buffer.buffer(), delay).map_err(|_| "Failed to update frame")?;
        self.epd.display_frame(spi, delay).map_err(|_| "Failed to display frame")?;
        self.epd.sleep(spi, delay).map_err(|_| "Failed to sleep display")?;
        Ok(())
    }

    fn show_inserted_amount(&mut self, spi: &mut SPI, delay: &mut DELAY, amount_string: &str) -> Result<(), Box<dyn std::error::Error>> {
        let mut display_buffer = Display2in13::default();
        display_buffer.set_rotation(DisplayRotation::Rotate90);
        display_buffer.clear(Color::White).map_err(|_| "Failed to clear buffer")?;

        let large_style = MonoTextStyle::new(&FONT_9X18_BOLD, Color::Black);
        
        Text::new("Inserted amount:", Point::new(10, 4), large_style).draw(&mut display_buffer)?;
        Text::new(amount_string, Point::new(35, 45), large_style).draw(&mut display_buffer)?;
        Text::new(" Press button\n to show QR code", Point::new(0, 85), large_style).draw(&mut display_buffer)?;

        self.epd.update_frame(spi, display_buffer.buffer(), delay).map_err(|_| "Failed to update frame")?;
        self.epd.display_frame(spi, delay).map_err(|_| "Failed to display frame")?;
        self.epd.sleep(spi, delay).map_err(|_| "Failed to sleep display")?;
        Ok(())
    }

    fn show_qr(&mut self, spi: &mut SPI, delay: &mut DELAY, qr_content: &str) -> Result<(), Box<dyn std::error::Error>> {
        let mut display_buffer = Display2in13::default();
        display_buffer.set_rotation(DisplayRotation::Rotate90);
        display_buffer.clear(Color::White).map_err(|_| "Failed to clear buffer")?;

        Text::new("Scan\n\nQR\n\ncode", Point::new(0, 20), MonoTextStyle::new(&FONT_9X18_BOLD, Color::Black)).draw(&mut display_buffer)?;
        // Placeholder QR content text
        Text::new(qr_content, Point::new(50, 50), MonoTextStyle::new(&FONT_6X10, Color::Black)).draw(&mut display_buffer)?;

        self.epd.update_frame(spi, display_buffer.buffer(), delay).map_err(|_| "Failed to update frame")?;
        self.epd.display_frame(spi, delay).map_err(|_| "Failed to display frame")?;
        self.epd.sleep(spi, delay).map_err(|_| "Failed to sleep display")?;
        Ok(())
    }

    fn clean(&mut self, spi: &mut SPI, delay: &mut DELAY) -> Result<(), Box<dyn std::error::Error>> {
        self.epd.clear_frame(spi, delay).map_err(|_| "Failed to clear frame")?;
        self.epd.display_frame(spi, delay).map_err(|_| "Failed to display frame")?;
        self.epd.sleep(spi, delay).map_err(|_| "Failed to sleep display")?;
        Ok(())
    }
}
