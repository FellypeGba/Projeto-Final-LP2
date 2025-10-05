# Rastreabilidade: Requisitos → Código

## Requisitos Gerais

G1 — Threads (uso de pthreads)
- Implementação:
  - `src/server.c` — loop de `accept()` que cria `pthread_create` para `client_thread`
  - `src/client.c` — thread `receive_thread` para receber broadcasts
  - `src/chat_server.c` — thread `broadcaster` (`pthread_create` em `chat_server_init`)
- Evidência:
  - `server_log.txt` contém entradas de novos clientes e mensagens.
  - Uso de `pthread_create` nos arquivos listados.
- Status: Implementado
- Observações: pthreads foram escolhidos por compatibilidade POSIX.

G2 — Exclusão mútua
- Implementação:
  - `src/chat_server.c` — `pthread_mutex_t clients_mtx` protege `clients[]`, `client_names[]` e (parcialmente) o `history[]`.
  - `src/threadsafe_queue.c` — mutex interno `q->mtx` protege a fila.
- Evidência:
  - Chamadas `pthread_mutex_lock` / `pthread_mutex_unlock` no código.

G3 — Semáforos e condvars
- Implementação:
  - `src/chat_server.c` — `sem_t slots` controla número máximo de clientes 
  - `src/threadsafe_queue.c` — `pthread_cond_t cond` usado em `mq_pop`/`mq_push` para produtor-consumidor.
- Evidência:
  - Código com `sem_init`, `sem_trywait`, `sem_post`; `pthread_cond_wait`, `pthread_cond_signal`.
- Observação: `sem_trywait` é usado para rejeitar conexões quando não há slots livres (comportamento não‑bloqueante).

G4 — Monitores
- Implementação:
  - `include/threadsafe_queue.h` + `src/threadsafe_queue.c` implementam a fila como um monitor (mutex + condvar + API `mq_push`/`mq_pop`).
  - `include/chat_server.h` + `src/chat_server.c` implementam o monitor `ChatServer` (lista de clientes, fila, broadcaster, semáforo, histórico, nomes).
- Evidência:
  - APIs públicas (`mq_init`, `mq_push`, `mq_pop`, `chat_server_init`, `chat_server_add_client`, `chat_server_enqueue_message`, etc.)

G5 — Sockets
- Implementação:
  - `src/server.c` — `socket()`, `bind()`, `listen()`, `accept()` e `recv()` no `client_thread`.
  - `src/client.c` — `socket()`, `connect()`, `send`/`recv` usados por cliente.
- Evidência:
  - Código em `server.c` e `client.c`, e logs de conexão em `server_log.txt`.

G6 — Gerenciamento de recursos
- Implementação:
  - Várias funções fazem cleanup na falha (ex.: `chat_server_init` faz cleanup em erros, `chat_server_shutdown` fecha fila, threads e libera memória).
  - Signal handler `sigint_handler` apenas set a flag e fecha o socket de escuta para permitir shutdown seguro em `main`.
- Evidência:
  - `chat_server_shutdown` fecha sockets, junta `broadcaster` e libera memória.

G7 — Tratamento de erros (mensagens amigáveis no CLI)
- Implementação:
  - Os `perror` / `fprintf` e logs `tslog_write` estão presentes em pontos de falha no `client.c` e `server.c`.
  - Mensagens de retorno de erro no cliente fazem `tslog_write` e `perror`.
- Evidência:
  - Código mostra validações em `socket()`, `connect()`, `pthread_create`, `malloc`.

G8 — Logging concorrente (lib `libtslog`)
- Implementação:
  - `include/tslog.h` e `src/tslog.c` fornecem logger thread-safe.
  - Chamadas `tslog_write(...)` espalhadas por `server.c`, `client.c`, `chat_server.c`, `threadsafe_queue.c`.
- Evidência:
  - `server_log.txt` e `client_log_*.txt` mostram logs de várias threads.
- Status: Implementado

---

## Requisitos do Tema

T1 — Servidor TCP concorrente aceitando múltiplos clientes
- Implementação:
  - `src/server.c` — aceita conexões em loop, cria uma thread por cliente.
  - `src/chat_server.c` — gerencia lista de clientes e vagas.
- Evidência:
  - `server_log.txt` com "Novo cliente conectado" e `chat_server_add_client` registrando.
- Status: Implementado

T2 — Cada cliente em thread; broadcast para demais
- Implementação:
  - `src/server.c` — `client_thread` lê e chama `chat_server_enqueue_message`.
  - `src/chat_server.c` — `broadcaster_func` consome a fila e usa `send_all` para enviar aos outros.
- Evidência:
  - `server_log.txt` com "Broadcast enviado (remetente=..., alvos=...)" e logs de mensagens recebidas.
- Status: Implementado

T3 — Logging concorrente (libtslog)
- Implementação: ver G8 acima.
- Evidência: `server_log.txt`.

T4 — Cliente CLI (conectar, enviar/receber)
- Implementação:
  - `src/client.c` implementa cliente que conecta, cria `receive_thread` e envia mensagens via `send_all`.
  - `test_clients.sh` automates simple tests.
- Evidência:
  - `client_log_<pid>.txt` criados ao rodar clientes; `test_clients.sh` gera mensagens de teste.

T5 — Proteção de estruturas compartilhadas (lista de clientes, histórico)
- Implementação:
  - `src/chat_server.c` protege `clients[]`, `client_names[]`, `history[]` com `clients_mtx`.
  - `src/threadsafe_queue.c` protege a fila com mutex/condvar.
- Evidência:
  - Chamadas de lock/unlock no código; histórico guardado no `ChatServer`.
---
