#include <LPC214x.h>
#include <stdio.h>
#include <string.h>

/**************** DEFINITIONS ****************/

#define LED1        (1<<16)
#define LED2        (1<<17)
#define LED3        (1<<18)
#define LED4        (1<<19)

#define ALL_LED     (LED1 | LED2 | LED3 | LED4)

#define SLAVE_ADDR  0x4E

char lcd_buf[32];

/**************** 3 SCHEDULED BELL TIMES ****************/

unsigned int bell_hour[3] = {8, 8, 8};
unsigned int bell_min[3]  = {1, 2, 3};

unsigned char bell_done[3] = {0,0,0};

volatile unsigned char manual_ring = 0;

/**************** DELAY ****************/

void delay_ms(unsigned int ms)
{
    unsigned int i,j;

    for(i=0;i<ms;i++)
    {
        for(j=0;j<6000;j++);
    }
}

/**************** I2C FUNCTIONS ****************/

void I2C_Init()
{
    PINSEL0 |= 0x00000050;

    I2C0SCLH = 75;
    I2C0SCLL = 75;

    I2C0CONSET = (1<<6);
}

void I2C_Start()
{
    I2C0CONSET = (1<<5);

    while(!(I2C0CONSET & (1<<3)));

    I2C0DAT = SLAVE_ADDR;

    I2C0CONCLR = (1<<5);
    I2C0CONCLR = (1<<3);

    while(!(I2C0CONSET & (1<<3)));
}

void I2C_Write(unsigned char data)
{
    I2C0DAT = data;

    I2C0CONCLR = (1<<3);

    while(!(I2C0CONSET & (1<<3)));
}

/**************** LCD FUNCTIONS ****************/

void LCD_Command(unsigned char cmd)
{
    I2C_Write((cmd & 0xF0) | 0x0C);
    I2C_Write((cmd & 0xF0) | 0x08);

    I2C_Write(((cmd<<4) & 0xF0) | 0x0C);
    I2C_Write(((cmd<<4) & 0xF0) | 0x08);

    delay_ms(2);
}

void LCD_Char(unsigned char data)
{
    I2C_Write((data & 0xF0) | 0x0D);
    I2C_Write((data & 0xF0) | 0x09);

    I2C_Write(((data<<4) & 0xF0) | 0x0D);
    I2C_Write(((data<<4) & 0xF0) | 0x09);

    delay_ms(2);
}

void LCD_String(char *str)
{
    while(*str)
    {
        LCD_Char(*str++);
    }
}

void LCD_Clear()
{
    LCD_Command(0x01);

    delay_ms(5);
}

void LCD_Init()
{
    delay_ms(20);

    I2C_Start();

    LCD_Command(0x02);
    LCD_Command(0x28);
    LCD_Command(0x0C);
    LCD_Command(0x06);
    LCD_Command(0x01);

    delay_ms(10);
}

/**************** RTC FUNCTIONS ****************/

void RTC_Init()
{
    PREINT  = 0x01C8;
    PREFRAC = 0x61C0;

    CCR = 0x02;

    /* INITIAL RTC TIME */

    HOUR = 8;
    MIN  = 0;
    SEC  = 0;

    CCR = 0x01;
}

/**************** UART FUNCTIONS ****************/

void UART_Init()
{
    PINSEL0 |= 0x00000005;

    U0LCR = 0x83;

    U0DLL = 78;
    U0DLM = 0;

    U0FDR = 0x10;

    U0LCR = 0x03;
}

void UART_TxChar(char ch)
{
    while(!(U0LSR & 0x20));

    U0THR = ch;
}

void UART_String(char *str)
{
    while(*str)
    {
        UART_TxChar(*str++);
    }
}

/**************** BELL FUNCTION ****************/

void Bell_Ring()
{
    unsigned int i;

    LCD_Clear();

    LCD_Command(0x80);
    LCD_String("BELL RINGING");

    for(i=0;i<15;i++)
    {
        IO0SET = ALL_LED;

        delay_ms(100);

        IO0CLR = ALL_LED;

        delay_ms(100);
    }

    IO0CLR = ALL_LED;

    LCD_Clear();
}

/**************** EXTERNAL INTERRUPT ****************/

void EINT0_ISR(void) __irq
{
    manual_ring = 1;

    EXTINT = 0x01;

    VICVectAddr = 0;
}

void EINT0_Init()
{
    PINSEL1 |= 0x00000001;

    EXTMODE  = 0x01;

    EXTPOLAR = 0x00;

    VICVectAddr0 = (unsigned int)EINT0_ISR;

    VICVectCntl0 = 0x20 | 14;

    VICIntEnable = (1<<14);
}

/**************** MAIN ****************/

int main()
{
    int i;

    VPBDIV = 0x01;

    /******** LED OUTPUT ********/

    IO0DIR |= ALL_LED;

    IO0CLR = ALL_LED;

    /******** INITIALIZE ********/

    I2C_Init();

    LCD_Init();

    RTC_Init();

    UART_Init();

    EINT0_Init();

    LCD_Clear();

    LCD_Command(0x80);
    LCD_String("SMART BELL");

    LCD_Command(0xC0);
    LCD_String("SYSTEM READY");

    delay_ms(2000);

    LCD_Clear();

    while(1)
    {
        /******** DISPLAY RTC TIME ********/

        LCD_Command(0x80);

        sprintf(lcd_buf,
               "TIME:%02d:%02d:%02d",
               HOUR,
               MIN,
               SEC);

        LCD_String(lcd_buf);

        /******** SHOW NEXT BELL TIME ********/

        LCD_Command(0xC0);

        if((HOUR < bell_hour[0]) ||
           ((HOUR == bell_hour[0]) &&
            (MIN < bell_min[0])))
        {
            sprintf(lcd_buf,
                   "NEXT:%02d:%02d ",
                   bell_hour[0],
                   bell_min[0]);
        }
        else if((HOUR < bell_hour[1]) ||
                ((HOUR == bell_hour[1]) &&
                 (MIN < bell_min[1])))
        {
            sprintf(lcd_buf,
                   "NEXT:%02d:%02d ",
                   bell_hour[1],
                   bell_min[1]);
        }
        else
        {
            sprintf(lcd_buf,
                   "NEXT:%02d:%02d ",
                   bell_hour[2],
                   bell_min[2]);
        }

        LCD_String(lcd_buf);

        /******** CHECK ALL 3 BELL TIMES ********/

        for(i=0;i<3;i++)
        {
            if((HOUR == bell_hour[i]) &&
               (MIN  == bell_min[i]) &&
               (SEC  == 0) &&
               (bell_done[i] == 0))
            {
                Bell_Ring();

                bell_done[i] = 1;
            }

            if(SEC != 0)
            {
                bell_done[i] = 0;
            }
        }

        /******** SW6 INTERRUPT ********/

        if(manual_ring == 1)
        {
            Bell_Ring();

            manual_ring = 0;
        }

        delay_ms(200);
    }
}
