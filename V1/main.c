#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>

#define data_stream 0x78
#define MAX_STARS 3
#define STATE_GAMEPLAY 0
#define STATE_GAMEOVER 1

static uint32_t next_random = 12345;
int score = 0;
volatile uint8_t reset_requested = 0;
volatile uint8_t game_state = STATE_GAMEPLAY;

typedef struct{
    uint8_t x_pos;
    uint8_t page;
} StarObject;

typedef struct{
    uint8_t column;
} ShipObject;

ShipObject ship;
StarObject star[MAX_STARS];
volatile uint8_t dynamic_score_keys[3] = {10, 10, 10};

void adc_Init(void);
uint16_t adc_read(void);
void uart_Init(void);
void uart_TX(char data);
void uart_print_number(uint16_t number);
void i2cInit(void);
void start_flag(void);
void stop_flag(void);
void data_write(uint8_t data);
void ssd1306Init(void);
void ssd1306Command(uint8_t data);
void ssd1306Clear(void);
void draw15PixelShip(uint8_t col, uint8_t is_erase);
void draw4x4Star(uint8_t col, uint8_t page, uint8_t is_erase);
void updateScoreKeys(int num);
uint8_t get_random_column(void);
long map(long x, long in_min, long in_max, long out_min, long out_max);
bool checkCollision(uint8_t ship_x, uint8_t ship_page, uint8_t star_x, uint8_t star_page);

const uint8_t game_font [20][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 0
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 1 = A
    {0x3F, 0x40, 0x80, 0x40, 0x3F}, // 2 = V
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // 3 = R
    {0x3E, 0x41, 0x49, 0x49, 0x2E}, // 4 = G
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 5 = M
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 6 = O
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 7 = E
    {0x24, 0x49, 0x49, 0x49, 0x32}, // 8 = S
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 9 = C
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 10 = 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 11 = 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 12 = 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 13 = 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 14 = 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 15 = 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 16 = 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 17 = 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 18 = 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}  // 19 = 9
};

void drawCharDirect(uint8_t page, uint8_t col, uint8_t font_index){
    ssd1306Command(0xB0 + page);
    ssd1306Command(0x00 + (col & 0x0F));
    ssd1306Command(0x10 + ((col >> 4) & 0x0F)); // Added missing upper nibble layout command
    start_flag();
    data_write(data_stream);
    data_write(0x40);
    for(uint8_t col_idx = 0; col_idx < 5; col_idx++){
        data_write(game_font[font_index][col_idx]);
    }
    data_write(0x00);
    stop_flag();
}

void printPhrase(uint8_t *phrase_array, uint8_t array_length, uint8_t start_col, uint8_t target_page){
    uint8_t base_left_col = start_col;
    for(uint8_t i = 0; i < array_length; i++){
        uint8_t token = phrase_array[i];
        if(token == 255){
            target_page += 2;
            start_col = base_left_col;
            continue;
        }
        if(token == 254){
            for(uint8_t d = 0; d < 3; d++){
                drawCharDirect(target_page, start_col, dynamic_score_keys[d]);
                start_col += 6;
            }
            continue;
        }
        drawCharDirect(target_page, start_col, token);
        start_col += 6;
    }
}

ISR(INT0_vect){
    if(game_state == STATE_GAMEOVER){
        reset_requested = 1;
    }
}

void adc_Init(void){
    ADMUX = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t adc_read(void){
    ADCSRA |= (1 << ADSC);
    while(ADCSRA & (1 << ADSC)){}
    return ADC;
}

void uart_Init(){
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    UBRR0H = (103 >> 8);
    UBRR0L = 103;
}

void uart_TX(char data){
    while(!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}

void i2cInit(void){
    TWBR = 72;
    TWSR = 0;
    TWCR = (1 << TWEN);
}

void start_flag(void){
    TWCR = (1 << TWSTA) | (1 << TWINT) | (1 << TWEN);
    while(!(TWCR & (1 << TWINT)));
}

void stop_flag(void){
    TWCR = (1 << TWSTO) | (1 << TWINT) | (1 << TWEN); // FIXED: Changed step setup to TWSTO configuration
    while(TWCR & (1 << TWSTO));
}

void data_write(uint8_t data){
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while(!(TWCR & (1 << TWINT))); // FIXED: Added missing hardware status wait gate
}

void ssd1306Command(uint8_t data){
    start_flag();
    data_write(data_stream);
    data_write(0x00);
    data_write(data);
    stop_flag();
}

void ssd1306Clear(void){ // FIXED: Typo removed here
    for(uint8_t p = 0; p < 8; p++){
        ssd1306Command(0xB0 + p);
        ssd1306Command(0x00);
        ssd1306Command(0x10);
        start_flag();
        data_write(data_stream);
        data_write(0x40);
        for(uint8_t col = 0; col < 128; col++){
            data_write(0x00);
        }
        stop_flag();
    }
}

void ssd1306Init(void){
    _delay_ms(100);
    ssd1306Command(0xAE);
    ssd1306Command(0xD5); ssd1306Command(0x80);
    ssd1306Command(0xA8); ssd1306Command(0x3F);
    ssd1306Command(0xD3); ssd1306Command(0x00);
    ssd1306Command(0x40);
    ssd1306Command(0x20); ssd1306Command(0x02);
    ssd1306Command(0xA0); ssd1306Command(0xC0);
    ssd1306Command(0xDA); ssd1306Command(0x12);
    ssd1306Command(0x81); ssd1306Command(0xCF);
    ssd1306Command(0xD9); ssd1306Command(0xF1);
    ssd1306Command(0xDB); ssd1306Command(0x40);
    ssd1306Command(0xA4); ssd1306Command(0xA6);
    ssd1306Command(0x8D); ssd1306Command(0x14);
    ssd1306Clear();
    _delay_ms(100);
    ssd1306Command(0xAF);
}

void updateScoreKeys(int num){
    uint8_t hundreds = (num / 100) % 10;
    uint8_t tens = (num / 10) % 10;
    uint8_t ones = num % 10;
    dynamic_score_keys[0] = 10 + hundreds;
    dynamic_score_keys[1] = 10 + tens;
    dynamic_score_keys[2] = 10 + ones;
}

void draw4x4Star(uint8_t col, uint8_t page, uint8_t is_erase){
    if (page > 7) return;
    ssd1306Command(0xB0 + page);
    ssd1306Command(0x00 + (col & 0x0F));
    ssd1306Command(0x10 + ((col >> 4) & 0x0F));
    start_flag();
    data_write(data_stream);
    data_write(0x40);
    for(uint8_t i = 0; i < 4; i++){
        data_write(is_erase ? 0x00 : 0x3C);
    }
    stop_flag();
}

uint8_t get_random_column(void){
    next_random = next_random * 1103515245 + 12345;
    uint8_t rand_val = (uint8_t)(next_random / 65536) % 32768;
    return rand_val % 109;
}

long map(long x, long in_min, long in_max, long out_min, long out_max){
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void draw15PixelShip(uint8_t col, uint8_t is_erase){
    ssd1306Command(0xB7);
    ssd1306Command(0x00 + (col & 0x0F));
    ssd1306Command(0x10 + ((col >> 4) & 0x0F));
    start_flag();
    data_write(data_stream);
    data_write(0x40);
    for(uint8_t i = 0; i < 15; i++){
        if(is_erase){
            data_write(0x00); // FIXED: Changed 0x40 to 0x00 to erase pixels properly
        }else{
            uint8_t ship_sprite[15] = {
                0xE0, 0xF0, 0xE0, 0x18, 0xFE, 0xFF, 0xFF, 0xFF, 0xFE, 0x18, 0xE0, 0xF0, 0xF0, 0xE0
            };
            data_write(ship_sprite[i]);
        }
    }
    stop_flag();
}

void externalInit(void){
    PORTD |= (1 << PORTD2);
    EICRA |= (1 << ISC01);
    EICRA &= ~(1 << ISC00);
    EIMSK |= (1 << INT0);
}

bool checkCollision(uint8_t ship_x, uint8_t ship_page, uint8_t star_x, uint8_t star_page){ // FIXED: Capitalization matched
    if(ship_page != star_page) return false;
    if((ship_x < (star_x + 4)) && ((ship_x + 15) > star_x)){
        return true;
    }
    return false;
}

int main(void){
    i2cInit();
    ssd1306Init();
    adc_Init();
    externalInit();
    
    uint8_t game_over_screen[17] = {4, 1, 5, 7, 0, 6, 2, 7, 3, 255, 8, 9, 6, 3, 7, 0, 254};
    uint8_t current_col = 0;
    
    ADCSRA |= (1 << ADSC);
    while(ADCSRA & (1 << ADSC)); // FIXED: Wrapped ADSC target check accurately
    next_random = ADC;
    
    star[0].x_pos = 20; star[0].page = 1;
    star[1].x_pos = 60; star[1].page = 2;
    star[2].x_pos = 100; star[2].page = 5;
    
    draw15PixelShip(current_col, 0);
    uint8_t star_timer = 0;
    
    sei();
    
    while(1){
        switch(game_state){
            case STATE_GAMEPLAY:{
                uint16_t potValue = adc_read();
                uint8_t target_col = (uint8_t)map(potValue, 0, 1023, 0, 109);
                
                if(target_col != current_col){
                    draw15PixelShip(current_col, 1);
                    current_col = target_col;
                    draw15PixelShip(current_col, 0);
                }
                
                star_timer++;
                if(star_timer >= 20){
                    star_timer = 0;
                    for(uint8_t i = 0; i < MAX_STARS; i++){
                        draw4x4Star(star[i].x_pos, star[i].page, 1);
                        star[i].page++;
                        
                        if(checkCollision(current_col, 7, star[i].x_pos, star[i].page)){
                            ssd1306Clear();
                            updateScoreKeys(score);
                            game_state = STATE_GAMEOVER;
                            break;
                        }
                        
                        if(star[i].page > 7){
                            star[i].page = 0;
                            star[i].x_pos = get_random_column();
                            score++;
                        }
                        draw4x4Star(star[i].x_pos, star[i].page, 0);
                    }
                }
                _delay_ms(10);
                break;
            }
            case STATE_GAMEOVER:
                printPhrase(game_over_screen, 17, 37, 2);
                
                if(reset_requested){
                    reset_requested = 0;
                    score = 0;
                    star[0].x_pos = 20; star[0].page = 1;
                    star[1].x_pos = 60; star[1].page = 2;
                    star[2].x_pos = 100; star[2].page = 5;
                    star_timer = 0;
                    ssd1306Clear();
                    current_col = (uint8_t)map(adc_read(), 0, 1023, 0, 109);
                    draw15PixelShip(current_col, 0);
                    game_state = STATE_GAMEPLAY;
                }
                _delay_ms(50);
                break;
        }
    }
}
