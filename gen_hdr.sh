#!/bin/bash

echo -e "const char index_html[] PROGMEM = { $(gzip -9 -c nginx/index.html | hexdump -v -e '1/1 "0x%02X, "') };\nconst size_t index_html_length = $(wc -c nginx/index.html | cut -d ' ' -f1);" > src/smartswitch.esp8266/index_html.h && \
echo -e "const char app_js[] PROGMEM = { $(gzip -9 -c nginx/app.js | hexdump -v -e '1/1 "0x%02X, "') };\nconst size_t app_js_length = $(wc -c nginx/app.js | cut -d ' ' -f1);" > src/smartswitch.esp8266/app_js.h

