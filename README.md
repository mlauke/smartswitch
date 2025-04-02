# Sonnen SmartSwitch
... self-made sonnen battery controlled switch which drives a "3,3kW Heater" (Heißwasserspeicher-Einbauheizung)

## Features
- accesses the local Heater system via LPB to get the hot water temparature (boilder)
- calculates battery capacity and surplus from solar forecast and longterm consumption

## Web-Interface

## Software
- smartswitch.esp8266.ino

## LPB
to control access the

## Sample Hardware
- Heater - https://www.austria-email.at/produkte/zubehoer/elektro-einbauheizung/reu-18-33/
- Hot Water Tank - https://www.austria-email.at/produkte/indirekt-beheizte-speicher/standspeicher/standspeicher-hr/hr-160/
- sonnen battery https://sonnen.de/stromspeicher/sonnenbatterie-10/
- esp8266 module
- ArduiBox housing
- DIN Rail Power supply
- AC mains Triac cicuit to drive a 1NO Relay (Schütz)

## Thoughts
Pmax = (Tj,max - Tamb,max) / Rth
P_loss = 1,5V * 14,3 A = 21,45W
Pmax = ( 140°C - 40°C ) / ( Theat °C/W ) = 22 W => Rth = ( 140°C - 40°C ) / 21,45W = 4,66°C/W
Rth = (RqJC Junction to case) + (Rkühlkörper) = 1°C/W + Rheatsink => Rheatsink = 3,66°C/W


## TODO
- build an GaN (HEMT) AC Power Converter to dynamically control the "waste" of surplus power
-