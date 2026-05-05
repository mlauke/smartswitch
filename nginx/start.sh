#!/bin/bash

docker run -d -p 8080:8080 -p 8081:8081 --name nginx \
  --restart unless-stopped \
  -v $(pwd $0):/usr/share/nginx/html:ro \
  -v $(pwd $0)/nginx.conf:/etc/nginx/nginx.conf:ro nginx
