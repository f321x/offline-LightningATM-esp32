// Common utility functions for Offline-LightningATM-esp32 Web Installer

// Update connection status indicator
function updateConnectionStatus(connected) {
    const statusElement = document.getElementById('connection-status');
    if (connected) {
        statusElement.style.backgroundColor = '#d4edda';
        statusElement.style.color = '#155724';
        statusElement.textContent = '● Connected';
    } else {
        statusElement.style.backgroundColor = '#e0e0e0';
        statusElement.style.color = '#888';
        statusElement.textContent = '● Not connected';
    }
}

// Update config mode status indicator
function updateConfigModeStatus(inConfigMode) {
    const statusElement = document.getElementById('config-mode-status');
    if (inConfigMode) {
        statusElement.style.backgroundColor = '#d4edda';
        statusElement.style.color = '#155724';
        statusElement.textContent = '● Config mode';
    } else {
        statusElement.style.backgroundColor = '#e0e0e0';
        statusElement.style.color = '#888';
        statusElement.textContent = '● No config mode';
    }
}
