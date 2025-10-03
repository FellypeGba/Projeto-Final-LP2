# Projeto Final LP2
Projeto Final da Disciplina de Linguagem de Programação 2 (ou Programação Concorrente e Distribuída).
Tema A: Servidor de Chat Multiusuário (TCP)


## Como Executar o Código:
### Requisitos:
Para executar este projeto, é necessário estar em uma Máquina Ubuntu para poder usar as bibliotecas POSIX.
Por fim, confirme que seu ambiente tenha o gcc e o make instalado.
Caso contrário, pode-se fazer o download deles através desses comandos:
``` bash
sudo apt update
sudo apt install build-essential
```
O pacote build-essential já traz gcc, make e as libs básicas.

### Fazendo uso do MakeFile:
Abra o terminal na pasta raiz do projeto e execute este comando:
``` bash
make
```
Esse passo criará o executável do main.

### Executando o arquivo

#### Fazendo um Teste Manual
Com o Make feito, você pode rodar o servidor através desse comando:
``` bash
./server
```
Com ele em execução, basta abrir um outro terminal ou fazer uma conexão a partir das informações do servidor.  
Então, é só executar o código do cliente neste outro terminal:
``` bash
./client
```
O log do servidor estará sendo escrito em **"server_log.txt"**  
Enquanto isso, os logs do clientes sempre são criados um para cada client rodado, no formato: **"client_log(PID).txt"**
### Teste Simulando Múltiplos clientes
Para simular múltiplos clientes de forma prática, execute o servidor como dito anteriormente.  
Porém, para a parte de clientes, basta executar estes comandos em um novo terminal:  
Dando permissão de execução:
``` bash
chmod +x test_clients.sh
```
Executanto o script:
``` bash
./test_clients.sh
```
Nesta abordagem, os logs dos clientes entrarão na pasta **client_logs**
