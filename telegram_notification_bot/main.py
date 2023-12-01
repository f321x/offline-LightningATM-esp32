import requests
from time import sleep

url = "https://legend.lnbits.com/api/v1/wallet"  # your lnbits instance URL
api_key = "ABCDEFG"	# your lnbits wallet api key (not admin!)

bot_token = '123123123:12312jn3123123kjn123f'	# telegram bot token, create bot with @botfather
chat_id = '12312313123'	# your telegram chat id, contact a chat id bot to get it

min_balance = 1000	# minimum balance at which you want to get notified
refresh_interval = 5600		# refresh interval in which tha balances are fetched in seconds


def get_wallet_balance():
    headers = {
        "X-Api-Key": api_key
    }
    response = requests.get(url, headers=headers)

    if response.status_code == 200:
        data = response.json()
        return data["balance"]
    else:
        return int(-1)

def send_telegram_message(token, chat_id, message):
    url = f'https://api.telegram.org/bot{token}/sendMessage'
    data = {'chat_id': chat_id, 'text': message}
    response = requests.post(url, data=data)
    return response.json()

def bot():
	previous_balance = 0
	while (True):
		balance = get_wallet_balance()
		if balance == -1:
			print("Balance check failed.")
			sleep(3600)
			continue
		else:
			balance = int(balance/1000)
			difference = previous_balance - balance
			if difference > 1:
				message = f"{difference} Sats have been withdrawn!"
				send_telegram_message(bot_token, chat_id, message)
			elif balance < min_balance:
				message = f"Only {balance} Sats left in ATM, refill!"
				send_telegram_message(bot_token, chat_id, message)
			previous_balance = balance
		sleep(refresh_interval)

bot()


