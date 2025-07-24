#!/bin/bash

SRC_DIR=smartswitch.espxx/include

echo "const char index_html[] PROGMEM = { $(gzip -9 -c nginx/index.html | hexdump -v -e '1/1 "0x%02X, "') };" > ${SRC_DIR}/index_html.h && \
echo "const char app_js[] PROGMEM = { $(gzip -9 -c nginx/app.js | hexdump -v -e '1/1 "0x%02X, "') };" > ${SRC_DIR}/app_js.h && \
echo "const char app_css[] PROGMEM = { $(gzip -9 -c nginx/app.css | hexdump -v -e '1/1 "0x%02X, "') };" > ${SRC_DIR}/app_css.h && \
echo "const char app_icon[] PROGMEM = { $(hexdump -v -e '1/1 "0x%02X, "' nginx/favicon.ico) };" > ${SRC_DIR}/app_icon.h
