use qrcode::{QrCode, EcLevel, Version};


#[derive(Debug)]
#[allow(dead_code)]
pub struct LNBitsConnection {
    pub base_url: String,
    pub page_base_url: String,
    pub atm_secret: String,
    pub currency: String,
}


impl LNBitsConnection {

    /// split lnbits device string into base_url, secret and currency
    /// e.g. https://XXXX.lnbits.com/fossa/api/v1/lnurl/XXXXX,XXXXXXXXXXXXXXXXXXXXXX,USD
    #[allow(dead_code)]
    pub fn from_device_string(device_string: &str) -> Result<Self, &'static str> {
        let parts: Vec<&str> = device_string.splitn(3, ',').collect();

        if parts.len() != 3 {
            return Err("Device string must contain exactly 3 comma-separated parts: base_url,secret,currency");
        }

        let base_url = parts[0].trim();
        let atm_secret = parts[1].trim();
        let currency = parts[2].trim();

        if base_url.is_empty() {
            return Err("Base URL cannot be empty");
        }

        if atm_secret.is_empty() {
            return Err("ATM secret cannot be empty");
        }

        if currency.is_empty() {
            return Err("Currency cannot be empty");
        }

        let api_pos = base_url.find("api").ok_or("'api' not found in base URL")?;
        let atm_page_base_url = format!("{}atm?lightning=", &base_url[..api_pos]);

        Ok(
            Self {
                base_url: base_url.to_string(),
                page_base_url: atm_page_base_url.to_string(),
                atm_secret: atm_secret.to_string(),
                currency: currency.to_string(),
            }
        )
    }
}


/// Generates a QR code from the given data string
/// Returns a 2D vector of booleans where true = black pixel, false = white pixel
#[allow(dead_code)]
pub fn generate_qrcode(data: &str) -> Result<Vec<Vec<bool>>, qrcode::types::QrError> {
    let code = QrCode::with_version(
        data.as_bytes(),
        Version::Normal(6),
        EcLevel::L,
    )?;

    // Get the QR code as a matrix
    let width = code.width();
    let mut matrix = Vec::with_capacity(width);

    for y in 0..width {
        let mut row = Vec::with_capacity(width);
        for x in 0..width {
            // true for dark modules (black), false for light modules (white)
            row.push(code[(x, y)] != qrcode::Color::Light);
        }
        matrix.push(row);
    }

    Ok(matrix)
}