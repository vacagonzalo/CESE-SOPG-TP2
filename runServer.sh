#!/bin/bash
export $(cat .env)
cd web
python2.7 -m CGIHTTPServer $PUERTO_WEB
