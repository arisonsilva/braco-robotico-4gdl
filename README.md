# 🤖 Braço Robótico de 4 Graus de Liberdade com Controle Híbrido

Este repositório contém o firmware (Arduino) e o fluxo de automação (Node-RED) do Trabalho de Conclusão de Curso (TCC) de Engenharia Eletrônica da UFRPE. O projeto consiste em um braço robótico de baixo custo com **Programação por Demonstração (PbD)**, controle **Mestre-Escravo** e teleoperação remota via **IoT (Web)**.

## 📋 Sumário
- [Características do Projeto](#-características-do-projeto)
- [Arquitetura de Hardware (Lista de Conexões)](#-arquitetura-de-hardware-lista-de-conexões)
- [Arquitetura de Software](#-arquitetura-de-software)
- [Como Utilizar](#-como-utilizar)
- [Autor](#-autor)

---

## ✨ Características do Projeto
- **Controle Mestre-Escravo:** Uma miniatura física equipada com 4 potenciômetros replica em tempo real os movimentos do braço principal.
- **Programação por Demonstração (PbD):** Modo de gravação que salva as trajetórias e o tempo de execução na memória do Arduino para reprodução autônoma em loop.
- **Controle Remoto IoT:** Interface Web (Dashboard) via Node-RED com sliders para controle de cada eixo em tempo real e botões de comando (Play, Pause, Stop).
- **Otimização de Memória:** Uso do tipo `byte` em vez de `int` para maximizar a capacidade de armazenamento de passos (economia de 50% de RAM).
- **Máquina de Estados Finitos:** Transições suaves e controle de estado não-bloqueante (usando `millis()`).

---

## 🔌 Arquitetura de Hardware (Lista de Conexões)

Abaixo estão listadas as conexões físicas entre os componentes no **Arduino Uno** e no **Driver PCA9685**.

### 1. Entradas (Sensores e Botões)
| Componente | Pino no Arduino | Função |
| :--- | :--- | :--- |
| Potenciômetro da Base | **A0** | Leitura do eixo de rotação da base (Mestre) |
| Potenciômetro do Ombro | **A1** | Leitura do eixo de inclinação do ombro (Mestre) |
| Potenciômetro do Cotovelo | **A2** | Leitura do eixo de inclinação do cotovelo (Mestre) |
| Potenciômetro do Punho | **A3** | Leitura do eixo de rotação do punho (Mestre) |
| Botão MODE | **D8** | Alterna entre modo Gravação (1 clique) e Play (2 cliques) |
| Botão GRIP | **D9** | Abre/Fecha a garra |
| Botão STOP | **D10** | Pausa/Retoma (1 clique) e Volta ao Manual (2 cliques) |

### 2. Barramento I2C (Comunicação Arduino ↔ PCA9685)
| Pino no Arduino | Pino no PCA9685 | Função |
| :--- | :--- | :--- |
| **A4** | **SDA** | Linha de Dados I2C |
| **A5** | **SCL** | Linha de Clock I2C |
| **5V** | **VCC** | Alimentação lógica do chip (5V) |
| **GND** | **GND** | Referência de terra (GND Comum) |

### 3. Saídas (Atuadores no PCA9685)
*Alimentados por fonte externa de 5V/3A conectada ao borne V+ do PCA9685.*

| Canal no PCA9685 | Servomotor | Eixo de Movimento |
| :--- | :--- | :--- |
| **CH0** | Servo MG996R (15kg) | Base |
| **CH1** | Servo MG996R (15kg) | Ombro |
| **CH2** | Servo MG996R (15kg) | Cotovelo |
| **CH3** | Servo 6221MG (20kg) | Punho |
| **CH4** | Micro Servo (4.5kg) | Garra |

### ⚠️ Aviso de Alimentação
- **NUNCA** alimente os servomotores diretamente pelos 5V do Arduino. O consumo de corrente pode danificar a placa.
- Utilize uma **fonte externa de 5V (mínimo 3A)** conectada ao borne `V+` e `GND` do PCA9685.
- **GND Comum:** O GND da fonte externa, o GND do PCA9685 e o GND do Arduino **devem estar interligados**.
- Recomenda-se o uso de um capacitor eletrolítico (1000 µF a 4700 µF) entre `V+` e `GND` do PCA9685 para absorver picos de corrente.

---

## 💻 Arquitetura de Software

O projeto é dividido em duas partes principais:

### 1. Firmware (Arduino)
O código (`braco_robotico.ino`) utiliza uma máquina de estados não-bloqueante:
- `MANUAL`: Leitura direta dos potenciômetros e movimento suave (interpolação).
- `RECORD`: Leitura dos sensores e gravação das posições + tempo decorrido (`dt`) em arrays.
- `PLAY`: Reprodução autônoma em loop, com lógica de aproximação suave no passo inicial.
- `RETORNANDO`: Transição suave ao sair do modo PLAY.

**Comunicação Serial:**
- Baud Rate: `115200`
- Comandos remotos recebidos no formato: `EIXO:ANGULO\n` (Ex: `B:120\n` para Base a 120 graus).
- Comandos de sistema: `REMOTO_ON\n`, `REMOTO_OFF\n`, `GRAVAR\n`, `PLAY\n`, `PAUSE\n`, `STOP\n`.

### 2. Interface Remota (Node-RED)
O arquivo `flow_node_red.json` contém o fluxo do Dashboard. Ele formata os dados dos sliders em strings seriais e gerencia a trava de segurança (*Safety Interlock*), impedindo que o controle remoto interfira na miniatura física sem a autorização explícita (botão Remoto ON).

---

## 🚀 Como Utilizar

1. **Hardware:** Monte o circuito conforme a tabela de conexões acima. Certifique-se de que o GND é comum e a fonte externa está ligada.
2. **Firmware:** Abra o arquivo `.ino` na Arduino IDE, selecione a placa "Arduino Uno" e a porta COM correta e faça o upload.
3. **Node-RED:** 
   - Instale o Node-RED no seu computador (`npm install -g --unsafe-perm node-red`).
   - Instale os módulos `node-red-node-serialport` e `@flowfuse/node-red-dashboard`.
   - Importe o arquivo `flow_node_red.json`, configure a porta COM correta no nó serial e clique em **Deploy**.
   - Acesse o Dashboard via navegador (ex: `http://localhost:1880/dashboard/braco`).

---

## 👨‍🎓 Autor

**Arison Roberto dos Santos Silva**
Estudante de Engenharia Eletrônica - Universidade Federal Rural de Pernambuco (UFRPE)
Trabalho de Conclusão de Curso (2026)

*Orientadora: Profa. Dra. Ania Lussón Cervantes*

---
*Este projeto foi desenvolvido para fins acadêmicos e educacionais.*
