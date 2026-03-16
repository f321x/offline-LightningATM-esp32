use embedded_hal::digital::{InputPin, OutputPin};
use std::thread;
use std::time::{Duration, Instant};

pub const COIN_MAP: [u64; 10] = [0, 0, 5, 10, 20, 50, 100, 200, 1, 2];

pub struct CoinDetector<P, M> {
    coin_pin: P,
    mosfet_pin: M,
}

impl<P: InputPin, M: OutputPin> CoinDetector<P, M> {
    pub fn new(coin_pin: P, mosfet_pin: M) -> Self {
        Self { coin_pin, mosfet_pin }
    }

    pub fn set_accepting(&mut self, accepting: bool) {
        if accepting {
            self.mosfet_pin.set_low().unwrap();
        } else {
            self.mosfet_pin.set_high().unwrap();
        }
    }

    /// Detect pulses from the coin acceptor.
    /// This is a blocking function intended to be run in a loop or thread.
    /// It returns the number of pulses detected after a timeout.
    /// If `stop_signal` is provided (e.g. from a button press), it can return early (0 pulses).
    pub fn detect_coin<F>(&mut self, should_stop: F) -> u32 
    where F: Fn() -> bool
    {
        let mut pulses = 0;
        let mut last_pulse_time = Instant::now();
        let mut pulse_started = false;
        
        // Enable acceptance
        self.set_accepting(true);
        
        // let entering_time = Instant::now(); // Unused
        
        loop {
            if should_stop() {
                self.set_accepting(false);
                return 0;
            }

            let is_low = self.coin_pin.is_low().unwrap_or(false);
            
            if is_low && !pulse_started {
                // Potential start of pulse
                thread::sleep(Duration::from_millis(35)); // Debounce
                if self.coin_pin.is_low().unwrap_or(false) {
                    // Confirmed pulse
                    pulses += 1;
                    pulse_started = true;
                    last_pulse_time = Instant::now();
                    
                    // Block coins while receiving pulses
                    self.set_accepting(false);
                }
            } else if !is_low && pulse_started {
                // Pulse ended
                pulse_started = false;
            }
            
            // Timeout logic
            if pulses > 0 && last_pulse_time.elapsed().as_millis() > 200 { // PULSE_TIMEOUT
                 break;
            }
            
            // Sleep briefly to yield
            thread::sleep(Duration::from_millis(1));
        }
        
        pulses
    }

    pub fn wait_for_event<F>(&mut self, check_button: F) -> CoinInteraction 
    where F: Fn() -> bool 
    {
        let pulses = self.detect_coin(&check_button);
        
        if check_button() {
            return CoinInteraction::Button;
        }
        
        if pulses > 0 && (pulses as usize) < COIN_MAP.len() {
             let val = COIN_MAP[pulses as usize];
             if val > 0 {
                 return CoinInteraction::Coin(val);
             }
        }
        
        CoinInteraction::None
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum CoinInteraction {
    Button,
    Coin(u64),
    None,
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;
    use std::rc::Rc;

    // Mock Pin
    #[allow(dead_code)]
    struct MockPin {
        state: Rc<RefCell<Vec<bool>>>, // Sequence of states to return
        idx: Rc<RefCell<usize>>,
    }

    impl embedded_hal::digital::ErrorType for MockPin {
        type Error = std::convert::Infallible;
    }

    impl InputPin for MockPin {
        fn is_high(&mut self) -> Result<bool, Self::Error> {
            let idx = *self.idx.borrow();
            let vec = self.state.borrow();
            if idx < vec.len() {
                *self.idx.borrow_mut() += 1;
                Ok(!vec[idx]) // Inverse of low
            } else {
                Ok(true) // Default High
            }
        }

        fn is_low(&mut self) -> Result<bool, Self::Error> {
             let idx = *self.idx.borrow();
            let vec = self.state.borrow();
            if idx < vec.len() {
                *self.idx.borrow_mut() += 1;
                Ok(vec[idx])
            } else {
                Ok(false) // Default High (so not low)
            }
        }
    }

    #[allow(dead_code)]
    struct MockOutput {
        state: Rc<RefCell<bool>>,
    }
    
    impl embedded_hal::digital::ErrorType for MockOutput {
        type Error = std::convert::Infallible;
    }

    impl OutputPin for MockOutput {
        fn set_low(&mut self) -> Result<(), Self::Error> {
            *self.state.borrow_mut() = false;
            Ok(())
        }
        fn set_high(&mut self) -> Result<(), Self::Error> {
            *self.state.borrow_mut() = true;
            Ok(())
        }
    }

    // Test logic is hard because detect_coin uses thread::sleep and Instant.
    // But we can check basic compilation and logic flow if we mock time or ignore it.
    // For now, just compilation of test module ensures traits are correct.
}
