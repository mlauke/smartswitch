# smartswitch
... self-made sonnen battery controlled switch which drives a "3,3kW Heißwasserspeicher-Einbauheizung"

## hardware
- heater - https://www.austria-email.at/produkte/zubehoer/elektro-einbauheizung/reu-18-33/
- hot water tank - https://www.austria-email.at/produkte/indirekt-beheizte-speicher/standspeicher/standspeicher-hr/hr-160/
- sonnen battery https://sonnen.de/stromspeicher/sonnenbatterie-10/
- esp8266 module
- triac/relay based power switch


Pmax
Pmax = (Tj,max - Tamb,max) / Rth
Pverlust = 1,5V * 14,3 A = 21,45W
Pmax = ( 140°C - 40°C ) / ( Theat °C/W ) = 22 W => Rth = ( 140°C - 40°C ) / 21,45W = 4,66°C/W
Rth = (RqJC Junction to case) + (Rkühlkörper) = 1°C/W + Rheatsink => Rheatsink = 3,66°C/W

23 W /
## software
- smartswitch.esp8266.ino
