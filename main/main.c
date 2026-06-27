#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "ssd1306.h"
#include "driver/gpio.h"
#include "servo.h"
#include "pot.h"

/*******************************************************************************
 * PROJETO: Braço Robótico
 * DESENVOLVEDOR: Thiago Albuquerque Moreira
 * ASSISTÊNCIA: Código desenvolvido com o auxílio da IA Claude
 
 * OBS.: Utilizei a IA para ajudar arrumar o código (debugar) e aprender como
 otimizar o código, sendo feito com "listas de exercicios" semelhantes ao que tem
 no código.
 *******************************************************************************/
  
//========CONFIGURAÇÃO DOS PINOS========
#define LED_INDICADOR 2
#define SERVO_PINO1 25
#define SERVO_PINO2 26
#define SERVO_PINO3 27
#define SERVO_PINO4 4
#define SERVO_PINO5 33 
#define POT1 ADC1_CHANNEL_4 // GPIO 32
#define POT2 ADC1_CHANNEL_3  // GPIO 39
#define POT3 ADC1_CHANNEL_6  // GPIO 34
#define POT4 ADC1_CHANNEL_7  // GPIO 35
#define I2C_MASTER_NUM I2C_NUM_0
#define BTN_CIMA    18
#define BTN_BAIXO   19
#define BTN_OK      23
#define BTN_GARRA   5 
//==============================================

//=====================INDICAÇÃO DE INICIALIZAÇÃO==================
// Função para piscar o LED
void pisca_led(int tempo, int vezes, int pino){
  gpio_set_direction(pino,GPIO_MODE_OUTPUT);
    for (int i = 0; i < vezes; i++)
    {
        gpio_set_level(pino,1);
        vTaskDelay(tempo / portTICK_PERIOD_MS);
        gpio_set_level(pino,0);                 
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
//=================================================================

//========= VARIÁVEIS GLOBAIS E ESTRUTURAS DE DADOS =========
SSD1306_t display;
int opcao_selecionada = 0; // Controla o índice do menu: 0 = Manual, 1 = Gravação, 2 = Reprodução
int posicoes_gravadas[10][5]; // Matriz para armazenar trajetórias  (até 10 passos para as 5 articulações)
int total_posicoes = 5;
int posicao_atual  = 0;
char texto[32]; // Buffer de strings temporário para formatar saídas no display

// Estado atual da garra (0 = Aberta (0 graus), 1 = Fechada (180 graus))
int garra_fechada = 0; 
//===========================================================

//=========================MENU=======================
// Atualiza a tela exibindo as opções do menu e posicionando a seta ">" no item selecionado
void mostrar_menu(void) {
    ssd1306_clear_screen(&display, false);

    // Cada linha: se for a selecionada, coloca ">" na frente
    if (opcao_selecionada == 0) ssd1306_display_text(&display, 0, "> Modo Manual",   13, false);
    else                        ssd1306_display_text(&display, 0, "  Modo Manual",   13, false);

    if (opcao_selecionada == 1) ssd1306_display_text(&display, 2, "> Modo Gravacao", 15, false);
    else                        ssd1306_display_text(&display, 2, "  Modo Gravacao", 15, false);

    if (opcao_selecionada == 2) ssd1306_display_text(&display, 4, "> Modo Reproduzir", 17, false);
    else                        ssd1306_display_text(&display, 4, "  Modo Reproduzir", 17, false);
}
//=================================================================

//========= MÁQUINA DE ESTADOS DO SISTEMA =========
typedef enum {
    MODO_MANUAL,
    MODO_MENU,
    MODO_GRAVACAO,
    MODO_REPRODUCAO
} ModoAtual;

ModoAtual modo_atual = MODO_MENU; // Inicializa o sistema exibindo o menu principal
//==================================================

//================CONFIRMAÇÃO DO BOTÃO (DEBOUNCE)=============================
// Armazenam o estado anterior das entradas para detectar apenas a borda de subida (o clique)
int estado_anterior_cima  = 0;
int estado_anterior_baixo = 0;
int estado_anterior_ok    = 0;
int estado_anterior_garra = 0;

// Função para evitar falsos acionamentos nos botões
int botao_pressionado(int pino, int *estado_anterior) {
    int estado_atual = gpio_get_level(pino);

    // Detecta se o botão mudou de solto (0) para pressionado (1)
    if (estado_atual == 1 && *estado_anterior == 0) {
        vTaskDelay(pdMS_TO_TICKS(80)); // Filtro de tempo (debounce), evita falso acionamtno
        if (gpio_get_level(pino) == 1) { // Confirma se o botão continua pressionado
            *estado_anterior = 1;
            return 1; // Retorna verdadeiro (botão pressionado)
        }
    }
    // Reseta o estado quando o botão é solto
    if (estado_atual == 0) {
        *estado_anterior = 0;
    }
    return 0;
}
//=================================================================

void app_main(void){
    //====================== CONFIGURAÇÃO DE PERIFÉRICOS =================================
    i2c_master_init(&display, 21, 22, -1); // Inicializa barramento I2C (SDA=21, SCL=22)
    ssd1306_init(&display, 128, 64);      // Configura resolução do display OLED
    ssd1306_clear_screen(&display, false);
    
    vTaskDelay(pdMS_TO_TICKS(100));

    // Inicialização do timer do LEDC e configuração dos pinos PWM de cada servo
    servo_init_timer();
    servo_configurar(SERVO_PINO1, LEDC_CHANNEL_0);
    servo_configurar(SERVO_PINO2, LEDC_CHANNEL_1);
    servo_configurar(SERVO_PINO3, LEDC_CHANNEL_2);
    servo_configurar(SERVO_PINO4, LEDC_CHANNEL_3);
    servo_configurar(SERVO_PINO5, LEDC_CHANNEL_4);

    // Configuração dos pinos digitais de entrada para os botões
    configuracao_botao(BTN_CIMA);
    configuracao_botao(BTN_BAIXO);
    configuracao_botao(BTN_OK);
    configuracao_botao(BTN_GARRA);
    
    // Inicialização e configuração dos canais de leitura dos potenciômetros (ADC)
    pot_init();
    pot_configurar(POT1);
    pot_configurar(POT2);
    pot_configurar(POT3);
    pot_configurar(POT4);
    //===================================================================

    // Move todos os servos para a posição inicial
    servo_mover(90, LEDC_CHANNEL_0);
    servo_mover(90, LEDC_CHANNEL_1);
    servo_mover(90, LEDC_CHANNEL_2);
    servo_mover(90, LEDC_CHANNEL_3);
    servo_mover(0,  LEDC_CHANNEL_4); // Garra aberta inicial

    // Arrays auxiliares para simplificar a varredura sequencial através de estruturas de repetição (for)
    adc1_channel_t pots[4] = {POT1, POT2, POT3, POT4};
    ledc_channel_t servos[5] = {LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3, LEDC_CHANNEL_4};

    pisca_led(500, 2, LED_INDICADOR); // Pisca o LED de status indicando o fim da inicialização
    int menu_atualizar = 1; // Flag lógica para gerenciar e forçar a atualização da tela do menu

    //====================== LOOP PRINCIPAL DA APLICAÇÃO =================================
    while (1) {

        // ====== BLOCO DO MODO MANUAL ======
        if (modo_atual == MODO_MANUAL) {
            ssd1306_clear_screen(&display, false);
            ssd1306_display_text(&display, 2, "  Modo Manual", 13, false);
            ssd1306_display_text(&display, 4, "  OK p/ voltar", 14, false);

            // Realiza a leitura analógica dos 4 potenciômetros, converte para graus e atualiza os servos
            for (int i = 0; i < 4; i++) {
                int graus = graus_pot(pots[i]);
                servo_mover(graus, servos[i]);
                printf("%d\n",   graus_pot(pots[i]));
            }

            // Lê o botão da garra
            if (botao_pressionado(BTN_GARRA, &estado_anterior_garra)) {
                garra_fechada = !garra_fechada;
            }
            // Move fisicamente a garra: 180 graus se estiver fechada, ou 0 graus se aberta
            servo_mover(garra_fechada ? 180 : 0, LEDC_CHANNEL_4);

            // Verifica clique no botão OK para sair do modo manual e retornar ao Menu Principal
            if (botao_pressionado(BTN_OK, &estado_anterior_ok)) {
                modo_atual = MODO_MENU;
                menu_atualizar = 1;
            }

        // ====== BLOCO DO MODO MENU ======
        } else if (modo_atual == MODO_MENU) {
            // Executa a atualização gráfica da tela apenas quando houver alteração de estado no menu
            if (menu_atualizar) {
                mostrar_menu();
                menu_atualizar = 0;
            }

            // Funçaõ do botão CIMA: decrementa o índice de seleção e faz o efeito de retornar pro final
            if (botao_pressionado(BTN_CIMA, &estado_anterior_cima)) {
                opcao_selecionada--;
                if (opcao_selecionada < 0) opcao_selecionada = 2;
                menu_atualizar = 1;
            }

            // Função do botão BAIXO: incrementa o índice de seleção e faz o efeito de retornar para o inicio
            if (botao_pressionado(BTN_BAIXO, &estado_anterior_baixo)) {
                opcao_selecionada++;
                if (opcao_selecionada > 2) opcao_selecionada = 0;
                menu_atualizar = 1;
            }

            // Função do botão OK: Altera o estado do sistema baseado na opção que estava selecionada antes
            if (botao_pressionado(BTN_OK, &estado_anterior_ok)) {
                if (opcao_selecionada == 0) modo_atual = MODO_MANUAL;
                if (opcao_selecionada == 1) modo_atual = MODO_GRAVACAO;
                if (opcao_selecionada == 2) modo_atual = MODO_REPRODUCAO;
                menu_atualizar = 1;
            }

        // ====== BLOCO DO MODO GRAVAÇÃO ======
        } else if (modo_atual == MODO_GRAVACAO) {
            // Formata e exibe o passo atual que está sendo gravado (ex: "Gravando: 1/5")
            sprintf(texto, "Gravando: %d/%d", posicao_atual + 1, total_posicoes);

            ssd1306_clear_screen(&display, false);
            ssd1306_display_text(&display, 2, texto, strlen(texto), false);

            // Atualiza os 4 servos normais em tempo real para permitir que o usuário posicione o braço livremente antes de salvar
            for (int i = 0; i < 4; i++) {
                int graus = graus_pot(pots[i]);
                servo_mover(graus, servos[i]);
            }

            // Permite abrir ou fechar a garra antes de salvar o passo corrente
            if (botao_pressionado(BTN_GARRA, &estado_anterior_garra)) {
                garra_fechada = !garra_fechada;
            }
            servo_mover(garra_fechada ? 180 : 0, LEDC_CHANNEL_4);

            vTaskDelay(pdMS_TO_TICKS(50));

            // Configura a  confirmação de captura: grava a posição atual de todas as articulações na matriz de memória
            if (botao_pressionado(BTN_OK, &estado_anterior_ok)){
                // Salva a posição angular dos 4 eixos principais
                posicoes_gravadas[posicao_atual][0] = graus_pot(POT1);
                posicoes_gravadas[posicao_atual][1] = graus_pot(POT2);
                posicoes_gravadas[posicao_atual][2] = graus_pot(POT3);
                posicoes_gravadas[posicao_atual][3] = graus_pot(POT4);
                posicoes_gravadas[posicao_atual][4] = (garra_fechada ? 180 : 0);

                ssd1306_clear_screen(&display, false);
                ssd1306_display_text(&display, 3, "Posicao salva!", 13, false);
                vTaskDelay(pdMS_TO_TICKS(800));

                posicao_atual++; // Avança para o próximo passo

                // Se atingir o limite estipulado de passos salvos, limpa as variáveis e retorna de forma automática ao menu
                if (posicao_atual == total_posicoes) {
                    posicao_atual = 0;
                    modo_atual = MODO_MENU;
                    menu_atualizar = 1;
                }
            }

        // ====== BLOCO DO MODO REPRODUÇÃO ======
        } else if (modo_atual == MODO_REPRODUCAO) {
            // Exibe na tela qual passo da sequência gravada está sendo executado no momento
            sprintf(texto, "Reprod.: %d/%d", posicao_atual + 1, total_posicoes);
            ssd1306_clear_screen(&display, false);
            ssd1306_display_text(&display, 2, texto, strlen(texto), false);

            // Carrega e move simultaneamente os 5 servos para os valores salvos na memória
            for (int i = 0; i < 5; i++) {
                servo_mover(posicoes_gravadas[posicao_atual][i], servos[i]);
            }

            vTaskDelay(pdMS_TO_TICKS(800)); // Tempo de espera para que os motores consigam alcançar fisicamente o ângulo salvo

            posicao_atual++; // Avança a sequência de reprodução
            
            // Loop infinito de trajetória: se chegar ao último passo, reinicia a rota a partir do primeiro passo (0)
            if (posicao_atual == total_posicoes) {
                posicao_atual = 0; // Continua o loop
            }

            // Verifica o clique do botão OK para interromper imediatamente a reprodução e retornar ao menu
            if (botao_pressionado(BTN_OK, &estado_anterior_ok)) {
                posicao_atual = 0;
                modo_atual = MODO_MENU;
                menu_atualizar = 1;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(25)); // Watchdog (tempo)
    }
}

```
