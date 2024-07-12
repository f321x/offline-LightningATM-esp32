use dotenv::dotenv;
use reqwest::blocking::Client;
use reqwest::header::{HeaderMap, HeaderValue};
use serde_json::Value;
use std::env;
use std::thread::sleep;
use std::time::Duration;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // dotenv().ok();

    let url = env::var("LNBITS_URL").expect("URL not set in .env file");
    let min_balance: i64 = env::var("MIN_BALANCE")
        .expect("MIN_BALANCE not set in .env file")
        .parse()?;
    let refresh_interval: u64 = env::var("REFRESH_INTERVAL")
        .expect("REFRESH_INTERVAL not set in .env file")
        .parse()?;
    let api_key = env::var("LNBITS_API_KEY").expect("LNBITS_API_KEY not set in .env file");
    let bot_token =
        env::var("TELEGRAM_BOT_TOKEN").expect("TELEGRAM_BOT_TOKEN not set in .env file");
    let chat_id = env::var("TELEGRAM_CHAT_ID").expect("TELEGRAM_CHAT_ID not set in .env file");

    let mut previous_balance: i64 = 0;

    loop {
        match get_wallet_balance(&api_key, &url) {
            Ok(balance) => {
                let balance = balance / 1000;
                let difference = previous_balance - balance;

                if difference > 1 {
                    let message = format!("{} Sats have been withdrawn!", difference);
                    send_telegram_message(&bot_token, &chat_id, &message)?;
                } else if balance < min_balance {
                    let message = format!("Only {} Sats left in ATM, refill!", balance);
                    send_telegram_message(&bot_token, &chat_id, &message)?;
                }

                previous_balance = balance;
            }
            Err(_) => {
                println!("Balance check failed.");
                sleep(Duration::from_secs(3600));
                continue;
            }
        }

        sleep(Duration::from_secs(refresh_interval));
    }
}

fn get_wallet_balance(api_key: &str, url: &str) -> Result<i64, Box<dyn std::error::Error>> {
    let client = Client::new();
    let mut headers = HeaderMap::new();
    headers.insert("X-Api-Key", HeaderValue::from_str(api_key)?);

    let response = client.get(url).headers(headers).send()?;

    if response.status().is_success() {
        let data: Value = response.json()?;
        Ok(data["balance"]
            .as_i64()
            .expect("Failed to unwrap balance from returned JSON"))
    } else {
        Err(format!("Failed to get balance, status code: {}", response.status()).into())
    }
}

fn send_telegram_message(
    token: &str,
    chat_id: &str,
    message: &str,
) -> Result<(), Box<dyn std::error::Error>> {
    let telegram_url: String = format!("https://api.telegram.org/bot{}/sendMessage", token);
    let params = [("chat_id", chat_id), ("text", message)];

    let client = Client::new();
    client.post(telegram_url).form(&params).send()?;

    Ok(())
}
