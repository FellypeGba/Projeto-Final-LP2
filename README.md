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
Após criar o arquivo, basta executar este comando:

``` bash
./main
```
### Instruções:
Com o código rodando, basta digitar a sua escolha no terminal.
Apertar 1 sobrescreverá os logs já existentes.
Já selecionar outra tecla escreverá os logs na próxima linha.
Depois de selecionado, o código escreverá os resultados no arquivo **"logs.txt"**
