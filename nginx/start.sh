#!/bin/bash

docker run --rm -p 8080:80 --name nginx \
  -v $(pwd $0):/usr/share/nginx/html:ro \
  -v $(pwd $0)/nginx.conf:/etc/nginx/nginx.conf:ro nginx
