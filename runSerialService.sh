#!/bin/bash
export $(cat .env)
cd SerialService
./clean.sh
./compilar.sh
./serialService $PUERTO_SERIE $BAUDIOS $PUERTO_TCP $IP_TCP