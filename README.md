# ButtonPresser

My server does not support power over ethernet. In case of a power outage, if I am not physically around, I need a way to physically press the power button. To do so, I use a small servo motor placed over the button, an Arduino running the code in this repo, and a static file hosted for free on a remote/cloud server.

The Arduino is set to constantly poll a hardcoded URL for a JSON config file. Based on the contents of this file, it will press the button for a specified duration. This poll-based approach allows the button presser to be controlled remotely without having to use port forwarding to push commands to the Arduino directly (there is also a push option though).

## Usage

1. On first boot, the Arduino creates a Wi-Fi access point. Connect to it and go to `192.168.4.1` and enter your LAN Wi-Fi credentials.
2. The Arduino will connect to your Wi-Fi network. Figure out the IP it has been assigned by your router and open it in a browser.
3. Ensure you have a config JSON file hosted at some public URL. See the page from step 3 for examples of what the config can look like.
4. Enter the URL to your JSON file and click save.
5. Update the file whenever you would like to issue a new command.

There are 2 types of commands:
1. `loop`: Continuously press the button on a loop.
2. `single`: Press once.