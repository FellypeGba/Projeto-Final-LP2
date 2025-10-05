#!/bin/bash

LOGDIR="client_logs"
mkdir -p "$LOGDIR"

# Número de clientes a iniciar (pode passar como primeiro argumento)
NUM_CLIENTS=${1:-5}

# Nome base para clientes (opcional segundo argumento)
BASE_NAME=${2:-Pessoa}

echo "Iniciando $NUM_CLIENTS clientes com base de nome '$BASE_NAME'..."
for i in $(seq 1 $NUM_CLIENTS); do
    NAME="${BASE_NAME}${i}"
    # Inicia cada cliente em background, envia uma mensagem e fecha
    echo "Oi de $NAME" | ./client "$NAME" &
    sleep 0.2
done

# Aguarda todos os clientes terminarem
wait

# Move todos os logs gerados para a pasta client_logs
mv client_log_*.txt "$LOGDIR/" 2>/dev/null || true
echo "Logs dos clientes movidos para $LOGDIR/"