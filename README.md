## Set up

Before compiling in arduino IDE:

1. Update [libraries](https://github.com/roy-mdr/es-iot-libs)
1. Change the *client id* in line 10 to match id in `controllers` database
1. Set the correct server at line 52 and 91

On PC:

1. Allow PC to Wake on LAN

On server:

1. Configure `devices` and `controllers` databases
    1. On `devices`: send an event-type `"wol"` and detail the MAC target in this format `XX:XX:XX:XX:XX:XX`
        ```json
        "execute": {
			"etype": "wol",
			"detail": "XX:XX:XX:XX:XX:XX"
		}
        ```
