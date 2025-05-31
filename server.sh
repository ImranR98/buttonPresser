#!/bin/bash
set -e
echo "Server IP: $(hostname -I | awk '{print $1}')"
docker run --rm -v ./public:/usr/share/nginx/html -p 8080:80 nginx:alpine  