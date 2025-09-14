//! logic for user configuration of the atm, persisted in esp flash

use crate::util::LNBitsConnection;

use esp_idf_svc::nvs::{EspDefaultNvsPartition, EspNvs, NvsDefault};


pub struct Config {
    nvs: EspNvs<NvsDefault>,
}

impl Config {
    pub fn open() -> Result<Self, esp_idf_svc::sys::EspError> {
        let nvs_partition = EspDefaultNvsPartition::take()?;
        let nvs = EspNvs::new(nvs_partition, "config", true)?;
        log::debug!("loaded config from storage");
        Ok(Self { nvs })
    }

    #[allow(dead_code)]
    pub fn persist_lnbits_connection(&mut self, connection: &LNBitsConnection) -> Result<(), esp_idf_svc::sys::EspError> {
        self.nvs.set_str("base_url", &connection.base_url)?;
        self.nvs.set_str("page_base_url", &connection.page_base_url)?;
        self.nvs.set_str("atm_secret", &connection.atm_secret)?;
        self.nvs.set_str("currency", &connection.currency)?;
        log::debug!("persisted lnbits connection to storage: {:?}", connection);
        Ok(())
    }

    #[allow(dead_code)]
    pub fn get_lnbits_connection(&self) -> Result<Option<LNBitsConnection>, esp_idf_svc::sys::EspError> {
        let mut buf = [0u8; 256];

        let base_url = match self.nvs.get_str("base_url", &mut buf)? {
            Some(s) => s.to_string(),
            None => return Ok(None),
        };

        let page_base_url= match self.nvs.get_str("page_base_url", &mut buf)? {
            Some(s) => s.to_string(),
            None => return Ok(None),
        };

        let atm_secret = match self.nvs.get_str("atm_secret", &mut buf)? {
            Some(s) => s.to_string(),
            None => return Ok(None),
        };

        let currency = match self.nvs.get_str("currency", &mut buf)? {
            Some(s) => s.to_string(),
            None => return Ok(None),
        };

        log::debug!("Loaded lnbits connection: {:?} | {:?} | {:?} | {:?}", base_url, page_base_url, atm_secret, currency);

        Ok(Some(LNBitsConnection {
            base_url,
            page_base_url,
            atm_secret,
            currency,
        }))
    }

    #[allow(dead_code)]
    pub fn persist_display_type(&mut self, display_type: &str) -> Result<(), esp_idf_svc::sys::EspError> {
        self.nvs.set_str("display_type", display_type)?;
        log::debug!("persisted display type: {}", display_type);
        Ok(())
    }

    #[allow(dead_code)]
    pub fn persist_board_type(&mut self, board_type: &str) -> Result<(), esp_idf_svc::sys::EspError> {
        self.nvs.set_str("board_type", board_type)?;
        log::debug!("persisted board type: {}", board_type);
        Ok(())
    }

    pub fn get_board_type(&self) -> Result<String, esp_idf_svc::sys::EspError> {
        let mut buf = [0u8; 64];
        match self.nvs.get_str("board_type", &mut buf)? {
            Some(s) => Ok(s.to_string()),
            None => Ok("Generic".to_string()), // Default
        }
    }

    pub fn get_display_type(&self) -> Result<String, esp_idf_svc::sys::EspError> {
        let mut buf = [0u8; 64];
        match self.nvs.get_str("display_type", &mut buf)? {
            Some(s) => Ok(s.to_string()),
            None => Ok("GxEPD2_150_BN".to_string()), // Default
        }
    }
}