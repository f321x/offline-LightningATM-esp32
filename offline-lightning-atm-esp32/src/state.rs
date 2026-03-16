
#[derive(Debug, Clone, PartialEq)]
pub enum AppState {
    Idle,
    CountingCoins(u64), // Amount in cents
    WithdrawReady(u64, String), // Amount, LNURL
    #[allow(dead_code)]
    Processing,
    #[allow(dead_code)]
    Shutdown,
}
