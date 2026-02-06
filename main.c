/* * PROJETO FINAL: Bomba de Infusão + LEDs de Status
 * ---------------------------------------------------------
 * Hardware: ATmega328p (16MHz)
 * Motor: PB0, PB1, PB2, PB3
 * LED VERDE (Ligado): PB4 (Pino 12)
 * LED VERMELHO (Travado): PB5 (Pino 13)
 * Botão Emergência: PD2 (Pino 2)
 * Potenciômetro: PC0 (A0)
 */

#define F_CPU 16000000UL // Define velocidade do chip para 16MHz

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// Sequência de Passo 
// 8 estados com mais precissão, (melhor do que 4)
const uint8_t passos[8] = {
	0b00001000, // 1. Bobina A (Norte)
	0b00001100, // 2. Bobina A + B (Nordeste) -> "Bobina Virtual"
	0b00000100, // 3. Bobina B (Leste)
	0b00000110, // 4. Bobina B + C (Sudeste)
	0b00000010, // 5. Bobina C (Sul)
	0b00000011, // 6. Bobina C + D (Sudoeste)
	0b00000001, // 7. Bobina D (Oeste)
	0b00001001  // 8. Bobina D + A (Noroeste)
};

//volatile: obriga a CPU a ir até o endereço físico da memória RAM e ler o valor real
volatile uint8_t indice_passo = 0;
volatile uint8_t sistema_ativo = 1; // 1 = Rodando, 0 = Emergência

void setup() {
    // --- 1. CONFIGURAÇÃO DE SAÍDAS (MOTOR + LEDS) ---
    // Configura PB0 a PB5 como SAÍDA
    // 0x3F em binário é 00111111 (1=Saída, 0=Entrada)
    DDRB |= 0x3F; 

    // --- 2. CONFIGURAÇÃO DO BOTÃO (EMERGÊNCIA) ---
    DDRD &= ~(1 << PD2); // PD2 como Entrada
    PORTD |= (1 << PD2); // Liga Pull-up Interno 
    EICRA |= (1 << ISC01); // Interrupção na borda de descida 
    EIMSK |= (1 << INT0);  // Habilita INT0

    // --- 3. CONFIGURAÇÃO DO ADC (POTENCIÔMETRO) ---
    // Ref=AVcc (5V). IMPORTANTE: Pinos A+ e Ar devem estar no 5V!
    ADMUX = (1 << REFS0); 
    // Habilita ADC e Prescaler 128
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); 

    // --- 4. CONFIGURAÇÃO DO TIMER1 (MOTOR) ---
    TCCR1B |= (1 << WGM12); // Modo CTC
    TCCR1B |= (1 << CS11) | (1 << CS10); // Prescaler 64
    TIMSK1 |= (1 << OCIE1A); // Habilita Interrupção por Comparação
    
    OCR1A = 20000; // Velocidade inicial
    
    sei(); // Habilita Interrupções Globais
}

// --- INTERRUPÇÃO DO MOTOR (RODA A CADA PASSO) ---
ISR(TIMER1_COMPA_vect) {
    if (sistema_ativo) {
        // Lógica dos LEDs + Motor:
        // (1 << PB4) -> Liga o LED Verde (bit 4)
        // | passos[...] -> Mistura com o passo do motor (bits 0-3)
        // O LED Vermelho (bit 5) fica 0 (apagado)
        
        PORTB = (1 << PB4) | passos[indice_passo];
        
        // Avança para o próximo passo
        indice_passo++;
        if (indice_passo > 7) indice_passo = 0;
    }
}

// --- INTERRUPÇÃO DE EMERGÊNCIA (BOTÃO APERTADO) ---
ISR(INT0_vect) {
    sistema_ativo = 0; // Trava a lógica do loop principal
    
    // ESTADO DE ERRO:
    // (1 << PB5) -> Liga SÓ o LED Vermelho
    // O resto (Motor e Verde) vira 0 (Desliga tudo)
    PORTB = (1 << PB5); 
}

int main(void) {
    setup();
    
    while(1) {
        if (sistema_ativo) {
            // 1. Ler Potenciômetro
            ADCSRA |= (1 << ADSC); // Inicia conversão
            while (ADCSRA & (1 << ADSC)); // Espera terminar
            
            uint16_t valor_adc = ADC;

            // 2. Calcular Velocidade (Fórmula Visual para Osciloscópio)
            // Lento (60000) -> Rápido (2000)
            uint32_t velocidade = 60000 - (valor_adc * 55); //55 É um (ganho) para ajustar a escala
            
            // 3. Limites de Segurança (Para não travar o Timer)
            if (velocidade < 2000) velocidade = 2000; // limite inferior
            if (velocidade > 60000) velocidade = 60000; // limite superior

            // 4. Atualiza Timer
            OCR1A = (uint16_t)velocidade;
            
            _delay_ms(10); // Estabilidade
        }
    }
}