/*
 *  Autor: Grupo de TCC - VIP
 *  Data: Julho, 2015
 *
 */
#include<pic16f628a.h>
#include <xc.h>
#include <stdio.h>

/*
 *  Endereços da EEPROM
 *  0x00 -> Configuração de Duty Cycle Motor Principal
 *  0x01 -> configuração de Tempo de atividade Motor Principal
 *  0x02 -> Configuração de Duty Cycle Motores Secundários
 *  0x03 -> Configuração de Tempo de atividade Motores Secundários
 */

// CONFIG
#pragma config FOSC = INTOSCIO  // Oscillator Selection bits (INTOSC oscillator: I/O function on RA6/OSC2/CLKOUT pin, I/O function on RA7/OSC1/CLKIN)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config MCLRE = ON       // RA5/MCLR/VPP Pin Function Select bit (RA5/MCLR/VPP pin function is MCLR)
#pragma config BOREN = OFF      // Brown-out Detect Enable bit (BOD disabled)
#pragma config LVP = OFF        // Low-Voltage Programming Enable bit (RB4/PGM pin has digital I/O function, HV on MCLR must be used for programming)
#pragma config CPD = OFF        // Data EE Memory Code Protection bit (Data memory code protection off)
#pragma config CP = OFF         // Flash Program Memory Code Protection bit (Code protection off)

#define _XTAL_FREQ 4000000

//Definição de Hardware para o LCD 16x2
#define Bit_enable PORTBbits.RB0 // Bit de Pulso de Enable
#define Bit_RS PORTBbits.RB2 //Bit de Comando (0) ou Dado (1)
#define Bus_data PORTB //PORT de Saída, utiliza os Bits de Rx4 á Rx7

//Definição de Hardware
#define Main_motor_left PORTAbits.RA2
#define Main_motor_right PORTAbits.RA1
#define Motor_1 PORTAbits.RA0
#define Motor_2 PORTAbits.RA6
#define Motor_3 PORTAbits.RA7
#define Botao_config PORTAbits.RA3
#define Botao_ajuste PORTAbits.RA4

//Configuração da variáveis
char reg;
bit flag_serial; //Variavel para saber se chegou algo na serial
bit Flag_active;
bit Flag_conectado;
bit Flag_motors_off; //Flag que habilita o modo de desativar os motores pelo tempo
bit display_config; //Exibe Mensagem de Configuração
bit display_value; // Exibe Valor da Configuração

int config; //Config terá 4 configurações de 1 á 4
int PWM_main_motor; //Duty Cycle do PWM do motor principal
int PWM_motors; //Duty Cycle do PWM dos outros motores
int Time_main_motor; //Tempo de atividade do motor principal
int Time_motors; //Tempo de atividade dos outros motores
int Active_motor; // Motor que será ativo

char Matrix_Char[4]; //Matriz que armazenara a Char para exibir no LCD
int aux_tm0; //Contador de 06 segundos Caso o usuario não aperte nada, o sistema volta a funcionar
int cnt_int_tmr0; //Contador da interrupção do timer 0 para desacionar os motores
int Tmp_atividade; //A variavel irá receber o tempo que os motores ficarão ativos


//Descrição de Funções do Sistema
void Setup(void); //Configuração inicial do Sistema
/*
 * Funções Relacionadas ao LCD
 */
void inicializa_lcd(void); //Função que inicializa o LCD
void lcd_comando(unsigned char dado); //Envia para o LCD um Comando
void lcd_escrevedado(unsigned char dado); // Envia ao LCD um dado
/*
 * Funções Relacionadas a EEPROM
 */
void Write_EEPROM(unsigned char addr, unsigned char data); // Escreve um dado na EEPROM
unsigned char Read_EEPROM(unsigned char addr); //Lê um dado na EEPROM
/*
 * Funções de Exibição no LCD
 */
void exibe_recebimento(void); // Função que exibe no LCD o dado que chegou na serial
void exibe_configuracao(void); // Exibe o modo de configuração do sistema
void exibe_config_valor(void); // Exibe o valor atual da configuração
void exibe_status(void); // Exibe status de Normal Funcionamento
/*
 * Funções Complementares
 */
bit debounce(unsigned char button); // Bounce para chegar se o botão esta realmente precionado
void IntToChar(int value); // Converte um valor interiro e grava em uma Matrix Char (4 algarismos)
void Duty_Pwm(int value); //Altera o DutyCycle do PWM em Porcentagem -> 0 a 100

void Setup(void) {
    // PCONbits.OSCF = 1; // 4MHz oscilador
    CMCON = 0x07; //Desabilita Comparadores Analógicos
    TRISA = 0x38; //Configura RA5,RA4,RA3 como entrada
    TRISB = 0x02; //Configura RB1 como entrada (RX)
    PORTA = 0x38;
    PORTB = 0x00;
    //Habilitando o modo PWM
    CCP1CONbits.CCP1M0 = 1;
    CCP1CONbits.CCP1M1 = 1;
    CCP1CONbits.CCP1M2 = 1;
    CCP1CONbits.CCP1M3 = 1;
    /* Seleciona o Periodo do PWM
     * PWM_periodo = (PR2+1) * 4 * (1/Clock) * Prescaler_Timer2
     * Frequencia escolhida 1000Hz
     * Clock 4MHz
     * Periodo = 0,001s
     * PR2 = 0xF9
     */
    //Configuração do Periodo do PWM
    //PR2 = 0xF9; //Periodo 1ms -> 1 KHz
    PR2 = 0x18; //Periodo 100us -> 10,0 KHz
    //PR2 = 0xBF; //Periodo 0,79ms -> 12,65Hz
    //PR2 = 0x0C; //Periodo 52us -> 19,23Khz

    //Setando o DutyCycle
    CCPR1L = 0xAF; // 8 bits mais Significativos
    //2 bits menos significativos
    CCP1CONbits.CCP1Y = 0; //
    CCP1CONbits.CCP1X = 0;

    //Seleciona o Prescaler do Timer2 1:4
    T2CONbits.T2CKPS0 = 1;
    T2CONbits.T2CKPS1 = 0;
    T2CONbits.TMR2ON = 1; //Habilita o Timer2

    /*
     *  Configurações da Serial
     */
    //Baud Rate
    SPBRG = 25; // Baud Rate 9600
    //SPBRG = 0;    // Baud Rate 115200

    //FLAGS de TX
    TXSTAbits.TX9 = 0; //Habilita USART com 8 bits
    TXSTAbits.TXEN = 0; //Desabilita o transmite da USART
    TXSTAbits.SYNC = 0; //Utiliza modo Assincrono na USART
    TXSTAbits.BRGH = 1; //Seleciona Baud Rate com velocidade baixa
    //FLAGS de RX
    RCSTAbits.SPEN = 1; //Habilita porta Serial
    RCSTAbits.RX9 = 0; //Recebimento de 8bits USART
    RCSTAbits.CREN = 1; //Habilita recebimento contínuo
    RCSTAbits.FERR = 0;
    RCSTAbits.OERR = 0;

    PIE1bits.RCIE = 1; //Habilita interrupção de recebimento USART
    PIR1bits.RCIF = 0; //Flag de interrupção de recebimento USART

    INTCONbits.GIE = 1; //Habilita chave geral interrupção
    INTCONbits.PEIE = 1; //Habilita interrupção de periféricos
    RCSTAbits.SPEN = 0; //Desabilita Serial
    inicializa_lcd(); //Inicializa LCD
    RCSTAbits.SPEN = 1; //Habilita Serial

    /*
     *  Fim das configurações da Serial
     */

    if (Read_EEPROM(0x7F) == 0xFF) {
        Write_EEPROM(0x00, 0x78);
        Write_EEPROM(0x01, 14);
        Write_EEPROM(0x02, 0x78);
        Write_EEPROM(0x03, 50);
        Write_EEPROM(0x7F, 0);
    }

    //Ao iniciar carregar valores Guardados na EEPROM
    PWM_main_motor = Read_EEPROM(0x00);
    Time_main_motor = Read_EEPROM(0x01);
    PWM_motors = Read_EEPROM(0x02);
    Time_motors = Read_EEPROM(0x03);

    //Habilitando Timer0
    INTCONbits.T0IE = 1; //Habilita interrupção do Timer0
    INTCONbits.T0IF = 0; //Limpa Flag do timer0
    OPTION_REGbits.T0CS = 0; //Uso do Clock Interno
    OPTION_REGbits.PSA = 0; //Uso do Prescaler para o timer0
    //Prescaler do Timer0 (Prescaler adotado => 256)
    OPTION_REGbits.PS0 = 1;
    OPTION_REGbits.PS1 = 1;
    OPTION_REGbits.PS2 = 1;

    Duty_Pwm(50);

    //Define contador do Timer0
    /*
     *  Formula : TRM0 = 256 - (Tempo Desejado * (Frequencia/4) * (1/Prescaler)
     *  Timer será de aproximadamente 50ms
     */
    TMR0 = 0x15; // Contador para 50ms com prescaler de 256
    config = 0; //Não Inicia Configuração
    display_config = 0; //Flag para exibição da configuração no LCD
    display_value = 0; //Flag para exibição do valor da configuração no LCD
    flag_serial = 0;
    reg = 0x00;
    aux_tm0 = 0; //Auxiliar dos 06 segundos da interrupção
    Flag_active = 0;
    Flag_conectado = 0;
    cnt_int_tmr0 = 0; //inicia o contador do timer 0 em zero
    Tmp_atividade = 0; //Tempo de atividade do motor inicia em zero
    Flag_motors_off = 0; //Desabilita Flag da interrupção por tempo
    Active_motor = 1; // Inicia girando o motor 1

    exibe_status();

}

void main(void) {
    Setup(); //Setup do Sistema

    while (1) {
        //Caso houver alteração no dado da Serial, exibe no Display pela função
        if (flag_serial) {
            exibe_recebimento(); //Função que exibe o dado que chegou na Serial
            flag_serial = 0; //Zera o Flag para não entrar novamente no Display
        }

        if (Flag_active) { // Caso a Flag de alguma ação for ativada

            RCSTAbits.SPEN = 0; //Desabilita a entrada Serial
            PIE1bits.RCIE = 0; //Desabilita interrupção de recebimento USART

            /*********************************************************************
             *
             *  Trata informações recebidas através da Serial
             *
             ********************************************************************/
            if (reg == 0x43) { //Tabela ASCII Letra C

                Flag_motors_off = 1; //Habilita o Flag da interrupção para desativar motores

                Duty_Pwm(PWM_motors); // Coloca o PWM com a config prévia

                if (Active_motor == 1) {
                    Motor_1 = 1;
                    Motor_2 = 0;
                    Motor_3 = 0;
                } else if (Active_motor == 2) {
                    Motor_1 = 0;
                    Motor_2 = 1;
                    Motor_3 = 0;
                } else if (Active_motor == 3) {
                    Motor_1 = 0;
                    Motor_2 = 0;
                    Motor_3 = 1;
                }
                Main_motor_right = 0; //Desabilita o motor principal para a direita
                Main_motor_left = 0; //Desabilita o motor principal para a esquerda

                Tmp_atividade = Time_motors; //Configura tempo de atividade
                cnt_int_tmr0 = 0; // Zera a flag do contador de tempo de atividade

            } else if (reg == 0x52) { //Tabela ASCII Letra R

                Flag_motors_off = 1; //Habilita o Flag da interrupção para desativar motores

                Duty_Pwm(PWM_main_motor); // Coloca o PWM com a config prévia
                Motor_1 = 0; //Desabilita motores
                Motor_2 = 0;
                Motor_3 = 0;
                Main_motor_right = 1; //Habilita o motor principal para a direita
                Main_motor_left = 0; //Desabilita o motor principal para a esquerda

                Active_motor++; // Incrementa o motor que será ativo no circle
                if (Active_motor > 3) Active_motor = 1;

                Tmp_atividade = Time_main_motor; //Configura tempo de atividade
                cnt_int_tmr0 = 0; // Zera a flag do contador de tempo de atividade


            } else if (reg == 0x4C) { //Tabela ASCII Letra L

                Flag_motors_off = 1; //Habilita o Flag da interrupção para desativar motores

                Duty_Pwm(PWM_main_motor); // Coloca o PWM com a config prévia
                Motor_1 = 0; //Desabilita motores
                Motor_2 = 0;
                Motor_3 = 0;
                Main_motor_right = 0; //Habilita o motor principal para a direita
                Main_motor_left = 1; //Desabilita o motor principal para a esquerda

                Active_motor--; // Incrementa o motor que será ativo no circle
                if (Active_motor < 1) Active_motor = 3;

                Tmp_atividade = Time_main_motor; //Configura tempo de atividade
                cnt_int_tmr0 = 0; // Zera a flag do contador de tempo de atividade

            } else if (reg == 0x54) { //Tabela ASCII Letra T

                Motor_1 = 0;
                Motor_2 = 0;
                Motor_3 = 0;
                Main_motor_right = 0; //Habilita o motor principal para a direita
                Main_motor_left = 0; //Desabilita o motor principal para a esquerda
            } else {
                PIE1bits.RCIE = 1; // Habilita interrupção de recebimento USART
                RCSTAbits.SPEN = 1; // Habilita novamente a entrada Serial
            }

            Flag_active = 0; //Limpa Flag para não executar o codigo novamente
            /*********************************************************************
             *
             *  Fim do tratamento da Serial
             *
             ********************************************************************/
        }

        //Caso o Flag de mostrar a configuração estiver zerado, exibe a config no lcd
        if (display_config) {
            exibe_configuracao(); //Chama Função de Exibição da Configuração
        }
        //Caso o Flag de mostrar o valor da configuração estiver zerado, exibe o valor da config no lcd
        if (display_value) {//Caso os valores das Configurações for alterado, exibir
            exibe_config_valor(); //Chama Função de Exibição dos valores de configuração
        }
    }
}

void interrupt interrupcao(void) {
    //Verifica se o FLAG da interrupção da USART esta ativo
    if (PIR1bits.RCIF == 1) {
        INTCONbits.GIE = 0; // Desabilita interrupção Global
        reg = RCREG; //Recebe o dado da serial e armazena no reg
        flag_serial = 1; // Flag que permite exibir no display o que recebeu
        Flag_active = 1; // Flag de tomada de Ação
        RCREG = 0x00; //Zera o Registro de entrada da serial para receber novo registro
        PIR1bits.RCIF = 0; //Limpa o FLAG da interrupção da USART
        INTCONbits.GIE = 1; // Habilita interrupção Global
    } else if (INTCONbits.T0IF) { //Testa a interrupção do Timer0
        aux_tm0++; // Contador para inatividade da configuração
        cnt_int_tmr0++; //Contador do tempo de atividade de motor
        INTCONbits.T0IE = 0; //Desabilita Interrupção do Timer0

        if (debounce(Botao_config)) {//Aplica um Debounce no Botão de Config RA3
            aux_tm0 = 0; //Zera contador de 06 segundos
            config++; //Incrementa o Botão da Configuração
            if (config > 5) { //Se Config maior que 5, zera o contador
                config = 0; //Zera Config
            }
            display_config = 1; // Após passar para esta rotina, pode exiber no display
            while (!Botao_config); //Enquanto o botão estiver precionado, segura aqui
        } else if (debounce(Botao_ajuste)) {//Aplica um Debounce no Botão de Config RA4
            aux_tm0 = 0; //Zera contador de 06 segundos
            if (config == 1) { //Caso a Configuração seja 1
                PWM_main_motor++; //Incrementa Contador do PWM do motor principal
                if (PWM_main_motor > 100) {// Caso PWM chegar á 1000, Zera o PWM
                    PWM_main_motor = 0;
                }
                Duty_Pwm(PWM_main_motor); //Atualiza PWM
            } else if (config == 2) {//Caso a Configuração seja 2
                Time_main_motor++; //Incrementa Contador do Tempo de atividade do motor principal
                if (Time_main_motor > 255) {// Caso Timer chegar á 256, Zera o timer
                    Time_main_motor = 0;
                }
            } else if (config == 3) {//Caso a Configuração seja 3
                PWM_motors++; //Incrementa Contador do PWM dos motores secundários
                if (PWM_motors > 100) {// Caso PWM chegar á 256, Zera o PWM
                    PWM_motors = 0;
                }
                Duty_Pwm(PWM_motors); //Atualiza PWM
            } else if (config == 4) {//Caso a Configuração seja 4
                Time_motors++; //Incrementa Contador do Tempo de atividade dos motores secundários
                if (Time_motors > 255) {// Caso Timer chegar á 256, Zera o timer
                    Time_motors = 0;
                }
            }
            display_value = 1; //Após passar aqui, permite que o display mostre o novo valor das configurações
        }

        //Caso o usuario fique 06 segundos sem apertar algum botão, o sistema retorno a operar
        if ((aux_tm0 == 100) && (config != 0)) {
            config = 0; //Zera a Flag de configuração
            aux_tm0 = 0; // Zera o contador de 06 segundos
            display_config = 1; // Após passar para esta rotina, pode exiber no display

            //Caso não seja precionado nenhum botão por 06 segundos, os valores não serão salvos
            PWM_main_motor = Read_EEPROM(0x00); // Retorna ultima configuração Salva
            Time_main_motor = Read_EEPROM(0x01); // Retorna ultima configuração Salva
            PWM_motors = Read_EEPROM(0x02); // Retorna ultima configuração Salva
            Time_motors = Read_EEPROM(0x03); // Retorna ultima configuração Salva
        }
        if ((cnt_int_tmr0 == Tmp_atividade) && (Flag_motors_off)) {
            cnt_int_tmr0 = 0; //Zera o contador
            //Desabilita todos o motores
            Motor_1 = 0;
            Motor_2 = 0;
            Motor_3 = 0;
            Main_motor_left = 0;
            Main_motor_right = 0;

            //Duty_Pwm(5); // Seta o Duty Cycle para 5%
            Flag_motors_off = 0; // Desabilita Flag para não entrar nesta interrupção
            reg = 0x00; // Limpa registrador que recebe a serial
            RCREG = 0x00; // Limpar buffer da Serial
            PIE1bits.RCIE = 1; // Habilita interrupção de recebimento USART
            RCSTAbits.SPEN = 1; // Habilita novamente a entrada Serial
        }

        TMR0 = 0x15; // Carrega Contador para 60ms com prescaler de 256
        INTCONbits.T0IF = 0; //Limpa Flag do Timer0
        INTCONbits.T0IE = 1; //Habilita Interrupção do Timer0
    }
}

void Duty_Pwm(int value) {
    // Função que altera o DutyCycle do PWM
    int porcent;

    T2CONbits.TMR2ON = 0; // Desabilita o Timer2

    porcent = (value * 10);
    CCPR1L = porcent >> 2; // 8 bits mais Significativos
    //2 bits menos significativos
    if (porcent & 0b00000010) {
        CCP1CONbits.CCP1X = 1;
    } else {
        CCP1CONbits.CCP1X = 0;
    }
    if (porcent & 0b0000001) {
        CCP1CONbits.CCP1Y = 1;
    } else {
        CCP1CONbits.CCP1Y = 0;
    }

    //Seleciona o Prescaler do Timer2 1:4
    T2CONbits.T2CKPS0 = 1;
    T2CONbits.T2CKPS1 = 0;
    T2CONbits.TMR2ON = 1; //Habilita o Timer2

}

void exibe_configuracao(void) {
    /*
     *  Configurações do Sistema (4 Configurações)
     *  1 -> Configura o Duty Cycle do PMW para o Motor Principal
     *  2 -> Configura o tempo de atividade do motor principal
     *  3 -> Configura o Duty Cycle do PMW para os 3 Motores Secundários
     *  4 -> Configura o tempo de atividade para os 3 Motores Secundários
     */
    display_value = 1; //Atualiza o Flag para exibir o vaor da configuração no LCD
    RCSTAbits.SPEN = 0; //Desabilita a Serial
    lcd_comando(0x01); //Limpa o LCD
    lcd_comando(0x80); //Posiciona Ponteiro na prmeira Linha e primeira coluna
    //Exibe no LCD
    lcd_escrevedado('C');
    lcd_escrevedado('o');
    lcd_escrevedado('n');
    lcd_escrevedado('f');
    lcd_escrevedado('i');
    lcd_escrevedado('g');
    lcd_escrevedado(' ');

    if ((config == 1) || (config == 2)) {
        lcd_escrevedado('M');
        lcd_escrevedado('o');
        lcd_escrevedado('t');
        lcd_escrevedado('o');
        lcd_escrevedado('r');
        lcd_escrevedado('0');
    } else if ((config == 3) || (config == 4)) {
        lcd_escrevedado('M');
        lcd_escrevedado('T');
        lcd_escrevedado(':');
        lcd_escrevedado('1');
        lcd_escrevedado('/');
        lcd_escrevedado('2');
        lcd_escrevedado('/');
        lcd_escrevedado('3');
    } else if (config == 5) {
        lcd_escrevedado('S');
        lcd_escrevedado('a');
        lcd_escrevedado('l');
        lcd_escrevedado('v');
        lcd_escrevedado('a');
        display_value = 0; //Atualiza o Flag para não exibir o vaor da configuração no LCD
        //Guarda Valores das Configurações na memória
        Write_EEPROM(0x00, PWM_main_motor);
        Write_EEPROM(0x01, Time_main_motor);
        Write_EEPROM(0x02, PWM_motors);
        Write_EEPROM(0x03, Time_motors);
        config = 0;
        __delay_ms(1500);
        exibe_status(); // Exibe Normal Funcionamento
    } else if (config == 0) {
        exibe_status(); // Exibe Normal Funcionamento
    }
    display_config = 0; //Atualiza o Flag para não exibir a mensagem novamente no LCD
    RCSTAbits.SPEN = 1; //Habilita a Serial
}

void exibe_status(void) {
    RCSTAbits.SPEN = 0; //Desabilita Serial
    lcd_comando(0x01); //Posiciona Ponteiro na prmeira Linha e primeira coluna
    lcd_comando(0x80); //Posiciona Ponteiro na prmeira Linha e primeira coluna
    if (Flag_conectado) {
        lcd_escrevedado('O'); //Exibe no LCD
        lcd_escrevedado('K');
    } else {
        lcd_escrevedado('A'); //Exibe no LCD
        lcd_escrevedado('g');
        lcd_escrevedado('u');
        lcd_escrevedado('a');
        lcd_escrevedado('r');
        lcd_escrevedado('d');
        lcd_escrevedado('.');
        lcd_escrevedado(' ');
        lcd_escrevedado('C');
        lcd_escrevedado('o');
        lcd_escrevedado('n');
    }
    RCSTAbits.SPEN = 1; //Habilita Serial
    display_value = 0; //Atualiza o Flag para não exibir o vaor da configuração no LCD
}

void exibe_config_valor(void) {
    RCSTAbits.SPEN = 0; //Desabilita a Serial
    lcd_comando(0xC0); //Posiciona Ponteiro na segunda Linha e primeira coluna
    if ((config == 1) || (config == 3)) {
        //Exibe no LCD
        lcd_escrevedado('V');
        lcd_escrevedado('e');
        lcd_escrevedado('l');
        lcd_escrevedado(':');
    } else if ((config == 2) || (config == 4)) {
        //Exibe no LCD
        lcd_escrevedado('T');
        lcd_escrevedado('e');
        lcd_escrevedado('m');
        lcd_escrevedado('p');
        lcd_escrevedado('o');
        lcd_escrevedado(':');
    }
    if (config == 1) {
        IntToChar(PWM_main_motor); //Converte o valor inteiro para Char
    } else if (config == 2) {
        IntToChar(Time_main_motor); //Converte o valor inteiro para Char
    } else if (config == 3) {
        IntToChar(PWM_motors); //Converte o valor inteiro para Char
    } else if (config == 4) {
        IntToChar(Time_motors); //Converte o valor inteiro para Char
    }
    //Exibe os valores da Char no LCD
    lcd_escrevedado(Matrix_Char[0]);
    lcd_escrevedado(Matrix_Char[1]);
    lcd_escrevedado(Matrix_Char[2]);
    lcd_escrevedado(Matrix_Char[3]);
    RCSTAbits.SPEN = 1; //Habilita a Serial
    display_value = 0; //Atualiza o Flag para não exibir o valor novamente no LCD
}

//Função que converte os valores de um numero em Char, retorna o numero na variavel Matrix_Char

void IntToChar(int value) {
    sprintf(Matrix_Char, "%4i", value);
}

void inicializa_lcd(void) {
    Bus_data = 0X00; //Zera o PORTB (Barramento D7~D0
    Bit_RS = 0; //Garante que  RS esteja em 0
    Bit_enable = 0; //Garante que  Enable esteja em OFF
    __delay_ms(15); //Delay de estabilidade
    lcd_comando(0x03); //Envia o comando de inicialização
    lcd_comando(0x03); //Envia o comando de inicialização
    lcd_comando(0x03); //Envia o comando de inicialização
    lcd_comando(0x02); //Comando que zera o contador e posiciona na primeira posição
    lcd_comando(0x28); // FUNCTION SET - Configura o LCD para 4 bits,
    lcd_comando(0x0C); // DISPLAY CONTROL - Display ligado, sem cursor
    lcd_comando(0x01); // Limpa o LCD
    lcd_comando(0x06); //ENTRY MODE SET - Desloca o cursor para a direita
}

void lcd_comando(unsigned char dado) {
    Bit_RS = 0; //Mostra que o dado será um Comando
    __delay_us(100); // Aguarda 100 us para estabilizar o pino do LCD
    Bit_enable = 0; // Desativa o bit de ENABLE
    //Envia a parte alta do dado garantindo que a parte baixa do Bus não se altere
    Bus_data = ((dado & 0xF0) | (Bus_data & 0x0F));
    Bit_enable = 1; //Ativa o Bit de Enable (ON)
    __delay_ms(5); //Delay de estabilidade
    Bit_enable = 0; //Desativa o Bit de Enable (OFF)
    //Envia a parte baixa do dado garantindo que a parte baixa do Bus não se altere
    Bus_data = ((dado << 4) | (Bus_data & 0x0F));
    Bit_enable = 1; //Ativa o Bit de Enable (ON)
    __delay_ms(5); //Delay de estabilidade
    Bit_enable = 0; //Desativa o Bit de Enable (OFF)
    __delay_us(40); // Aguarda 40us para estabilizar o LCD
}

void lcd_escrevedado(unsigned char dado) {
    Bit_RS = 1; //Mostra que o dado será um Char no LCD
    __delay_us(100); // Aguarda 100 us para estabilizar o pino do LCD
    Bit_enable = 0; // Desativa o bit de ENABLE
    //Envia a parte alta do dado garantindo que a parte baixa do Bus não se altere
    Bus_data = ((dado & 0xF0) | (Bus_data & 0x0F));
    Bit_enable = 1; //Ativa o Bit de Enable (ON)
    __delay_ms(5); //Delay de estabilidade
    Bit_enable = 0; //Desativa o Bit de Enable (OFF)
    //Envia a parte baixa do dado garantindo que a parte baixa do Bus não se altere
    Bus_data = ((dado << 4) | (Bus_data & 0x0F));
    Bit_enable = 1; //Ativa o Bit de Enable (ON)
    __delay_ms(5);
    Bit_enable = 0; //Desativa o Bit de Enable (OFF)
    __delay_us(40); // Aguarda 40us para estabilizar o LCD
}

void exibe_recebimento(void) {
    RCSTAbits.SPEN = 0; //Desabilita a Serial
    lcd_comando(0x01); //Limpa o LCD
    lcd_comando(0x80); //Posiciona o ponteiro na primeira lina
    if (reg == 0x43) { //Tabela ASCII Letra C
        lcd_escrevedado('C');
        lcd_escrevedado('i');
        lcd_escrevedado('r');
        lcd_escrevedado('c');
        lcd_escrevedado('l');
        lcd_escrevedado('e');
    } else if (reg == 0x52) { //Tabela ASCII Letra R
        lcd_escrevedado('S');
        lcd_escrevedado('w');
        lcd_escrevedado('i');
        lcd_escrevedado('p');
        lcd_escrevedado('e');
        lcd_escrevedado('-');
        lcd_escrevedado('R');
    } else if (reg == 0x4C) { //Tabela ASCII Letra L
        lcd_escrevedado('S');
        lcd_escrevedado('w');
        lcd_escrevedado('i');
        lcd_escrevedado('p');
        lcd_escrevedado('e');
        lcd_escrevedado('-');
        lcd_escrevedado('L');
    } else if (reg == 0x54) { //Tabela ASCII Letra T
        lcd_escrevedado('T');
        lcd_escrevedado('a');
        lcd_escrevedado('p');
    } else if (reg == 0x6B) { //Tabela ASCII Letra k
        lcd_escrevedado('L');
        lcd_escrevedado('e');
        lcd_escrevedado('a');
        lcd_escrevedado('p');
        lcd_escrevedado(' ');
        lcd_comando(0xC0);
        lcd_escrevedado('O');
        lcd_escrevedado('K');
        Flag_conectado = 1;
    } else {
        lcd_escrevedado('U');
        lcd_escrevedado('n');
        lcd_escrevedado('k');
        lcd_escrevedado('n');
        lcd_escrevedado('o');
        lcd_escrevedado('w');
        lcd_escrevedado('n');
        lcd_escrevedado(':');
        lcd_escrevedado(reg);
    }
    RCSTAbits.SPEN = 1; //Habilita a Serial
}

void Write_EEPROM(unsigned char addr, unsigned char data) {
    int aux_Global_interrupt;

    aux_Global_interrupt = INTCONbits.GIE; //Recebe a configuração da Interrupção Global
    EEADR = addr; //Aponta para o endereço de Gravação
    EEDATA = data; //Registrador EEDATA recebe o conteudo

    if (INTCONbits.GIE) {
        INTCONbits.GIE = 0; //Desabilita provisoriamente Interrupção Global caso esteja ativo
    }
    EECON1bits.WREN = 1; //Habilita o processo de escrita na EEPROM
    EECON2 = 0x55; //Processo Requerido para a Escrita (Datasheet)
    EECON2 = 0xAA;
    ; //Processo Requerido para a Escrita (Datasheet)
    EECON1bits.WR = 1; //Inicia a Escrita na EEPROM
    while (EECON1bits.WR); //Aguarda enquanto acontece a escrita (Cleared By Hardware)
    EECON1bits.WREN = 0; //Desabilita o processo de escrita na EEPROM
    if (aux_Global_interrupt) INTCONbits.GIE = 1; //Ativa a chave Geral de int, se estivesse ativa
}

unsigned char Read_EEPROM(unsigned char addr) {
    EEADR = addr; //Registrador recebe o Endereço a ser lido
    EECON1bits.RD = 1; //Habilita processo de Leitura (Cleared By Hardware)
    while (EECON1bits.RD); //Aguarda enquanto acontece a leitura
    return (EEDATA); //Retorna o valor lido na memoria
}

bit debounce(unsigned char button) {
    __delay_ms(30); //Delay de 30ms para evitar oscilações
    if (button == 0) {//Confirma se o botão ainda encontra-se em nível baixo
        return 1; //Retorna 1 caso o Botão esteja realmente precionado
    } else {
        return 0; //Retorna 0 caso seja uma oscilação transitória
    }
}