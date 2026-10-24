# RoboStronda

Projeto da disciplina **Projeto em Sistemas Embarcados**, do curso de Engenharia da Computação da UNIVAP.

**Integrantes:** Diogo Gil, Eduardo Peron e Gustavo Albano Nunes.

## Objetivo

Desenvolver um robô de combate controlado remotamente, com evolução para um robô de sumô autônomo.

## Tecnologias

- ESP32
- Linguagem C
- ESP-IDF
- PlatformIO
- Ponte H TB6612FNG e motores DC

## Funcionamento desta versão

O ESP32 cria a rede Wi-Fi `RoboStronda`. Ao conectar-se à rede, é possível acessar o controle pelo navegador em `http://192.168.4.1`.

O controle disponibiliza três comandos: **RODA DIREITA**, **RODA ESQUERDA** e **AMBOS**. Cada acionamento liga os motores selecionados por aproximadamente 2 segundos.

## Arquivos

- `platformio.ini`: configurações do PlatformIO
- `CMakeLists.txt`: configuração principal do projeto ESP-IDF
- `src/CMakeLists.txt`: registro do componente principal
- `src/main.c`: código do ESP32 e servidor web

## Execução

Abra a pasta do projeto no VS Code com a extensão PlatformIO, conecte o ESP32 e use **Build** e **Upload**. A porta configurada no arquivo `platformio.ini` é `COM5` e deve ser ajustada conforme o computador.

> Código disponibilizado como versão do projeto acadêmico. O envio ao GitHub não comprova compilação nem teste físico desta versão.
