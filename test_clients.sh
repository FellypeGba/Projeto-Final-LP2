#!/bin/bash

LOGDIR="client_logs"
mkdir -p "$LOGDIR"

NUM_CLIENTS=5 # Número de clientes a serem iniciados

for i in $(seq 1 $NUM_CLIENTS); do
    # Inicia cada cliente em background, redirecionando stdin para evitar travar
    echo "Passando aqui no servidor para dar um oi" | ./client &
    sleep 0.2
done

# Aguarda todos os clientes terminarem (opcional, depende do seu teste)
wait

# Move todos os logs gerados para a pasta client_logs
mv client_log_*.txt "$LOGDIR/"
echo "Logs dos clientes movidos para $LOGDIR/"