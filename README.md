# Braço Robótico S0-ARM101 com ESP32 

Este projeto consiste no desenvolvimento e controle de um braço robótico inspirado na arquitetura **S0-ARM101**. O sistema é controlado remotamente em tempo real por meio de um dispositivo físico com potenciômetros e conta com um **"Modo Sombra"**, capaz de gravar e reproduzir trajetórias automaticamente em looping.

O projeto foi validado por meio de simulação no Wokwi e posteriormente montado em um protótipo físico utilizando o framework **ESP-IDF** .

---

## 🚀 Funcionalidades

* **Modo Manual:** Controle direto e em tempo real de cada articulação e da garra através de potenciômetros e botões.
* **Modo Gravação:** Gravação sequencial de até 5 posições distintas na memória (expansível até 10 passos).
* **Modo Reprodução:** Execução contínua e automática em looping da trajetória que foi gravada.
* **Interface IHM:** Menu de navegação dinâmico exibido em um Display OLED com seleção via botões físicos.

---

## 🛠️ Hardware Utilizado

* **Processamento:** 1x ESP32
* **Atuadores:** 4x Servomotores (Articulações) e 1x Servo da Garra
* **Controle Analógico:** 4x Potenciômetros de precisão
* **Interface (IHM):** 1x Display OLED SSD1306 e 4x Botões Push-Button
* **Indicadores:** 1x LED de status do sistema

---

## 💻 Estrutura do Software

O firmware foi desenvolvido inteiramente em **C** utilizando o framework oficial da Espressif, o **ESP-IDF**. A arquitetura foi estruturada com base nas diretrizes do **FreeRTOS**, dividindo-se logicamente em:

1. **Camada de Aplicação:** Máquina de estados (`MODO_MANUAL`, `MODO_MENU`, `MODO_GRAVACAO` e `MODO_REPRODUCAO`) e o Buffer para armazenamento
