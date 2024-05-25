#include "Arduino.h"
#include "Wire.h"
#include "SSD1306Wire.h"
#include "esp_system.h"

#define FRQ_CNT_GPIO_CHN0 1 // GPIO引脚号
#define SDA_GPIO 19         // SDA 的 GPIO引脚号
#define SCL_GPIO 20         // SCL 的 GPIO引脚号
#define ROW_NUM 4
#define COL_NUM 4

int Fix_State = 0;
int Rock_Frequence = 0;
int Scissors_Frequence = 0;
int Paper_Frequence = 0;
int UI_NUM = 1;
int Last_Key = -1;
int Game_Start = 0;
int UserChoice = 0;
const int row_pins[ROW_NUM] = {4, 5, 6, 7};
const int col_pins[COL_NUM] = {11, 12, 13, 14};

const uint8_t rock_bitmap[] = {
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000011,
    0b01100000, 0b00011011,
    0b01101100, 0b00011011,
    0b11111100, 0b11011111,
    0b11111100, 0b11011111,
    0b11111100, 0b11111111,
    0b11111100, 0b01111111,
    0b11111000, 0b00111111,
    0b11100000, 0b00011111,
    0b11000000, 0b00000111,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
};

const uint8_t scissors_bitmap[] = {
    0b00000000, 0b00000000,
    0b01100000, 0b00000000,
    0b01100000, 0b01100000,
    0b01100000, 0b01100000,
    0b11000000, 0b00110000,
    0b11000000, 0b00110000,
    0b10000000, 0b00011001,
    0b10000000, 0b00011001,
    0b11110000, 0b00111111,
    0b11111000, 0b01111111,
    0b11111000, 0b01111111,
    0b11111000, 0b01111111,
    0b11110000, 0b00111111,
    0b11100000, 0b00011111,
    0b11000000, 0b00000111,
    0b00000000, 0b00000000,
};

const uint8_t paper_bitmap[] = {
    0b00000000, 0b00000000,
    0b00000000, 0b00000011,
    0b01100000, 0b00011011,
    0b01100000, 0b00011011,
    0b01100000, 0b00011011,
    0b01101100, 0b00011011,
    0b01101100, 0b00011011,
    0b01101100, 0b11011011,
    0b11111100, 0b11011111,
    0b11111100, 0b11011111,
    0b11111100, 0b11111111,
    0b11111100, 0b01111111,
    0b11111000, 0b00111111,
    0b11100000, 0b00011111,
    0b11000000, 0b00000111,
    0b00000000, 0b00000000,
};

const uint8_t question_mark_bitmap[] = {
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b11000000, 0b00000011,
    0b01110000, 0b00001110,
    0b00011000, 0b00011000,
    0b00001100, 0b00110000,
    0b00011000, 0b00011000,
    0b01110000, 0b00000000,
    0b11000000, 0b00000000,
    0b10000000, 0b00000001,
    0b10000000, 0b00000001,
    0b10000000, 0b00000001,
    0b00000000, 0b00000000,
    0b10000000, 0b00000001,
    0b10000000, 0b00000001,
    0b00000000, 0b00000000,
};

unsigned int CountingValue_CHN0;
float TimeInterval;  // 单位是 秒
float FrqValue_CHN0; // 单位是 Hz

void OLED_Display_UI1(void *ptParam);
void OLED_Display_UI2(void *ptParam);
void OLED_Diplay(void *ptParam);
void Frq_Meter(void *ptParam);
void UART0_Printer(void *ptParam);
void Pulse_Counting_Chn0(void);
int Scan_Keypad(void *ptParam);
int Key_Action(void *ptParam);
int User_Choice();
void Display_Win();
void Display_Lose();
void Display_Draw();
void Fix_Frequence(void *ptParam);
void Frequence_Set(int *Name, int Status);

SSD1306Wire display(0x3C, SDA_GPIO, SCL_GPIO);

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    display.init();                            // 初始化OLED屏
    display.setTextAlignment(TEXT_ALIGN_LEFT); // 向左对齐
    display.flipScreenVertically();            // 垂直翻转

    TimeInterval = 1;
    FrqValue_CHN0 = 0;
    CountingValue_CHN0 = 0;

    pinMode(FRQ_CNT_GPIO_CHN0, INPUT);
    /*
    开启外部中断 attachInterrupt(pin,function,mode);
    参数:
        pin: 外部中断引脚
        function: 外部中断回调函数
        mode: 5种外部中断模式, 见下表:
            RISING   上升沿触发
            FALLING  下降沿触发
            CHANGE   电平变化触发
            ONLOW    低电平触发
            ONHIGH   高电平触发
    */
    for (int i = 0; i < ROW_NUM; i++)
    {
        pinMode(row_pins[i], OUTPUT);
        digitalWrite(row_pins[i], HIGH);
    }

    // 初始化列引脚为输入，并启用上拉电阻
    for (int i = 0; i < COL_NUM; i++)
    {
        pinMode(col_pins[i], INPUT_PULLUP);
    }

    attachInterrupt(FRQ_CNT_GPIO_CHN0, Pulse_Counting_Chn0, CHANGE);

    xTaskCreatePinnedToCore(Frq_Meter, "Task FrqMeter", 2048, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(UART0_Printer, "Task UART0", 2048, NULL, 1, NULL, 0);
}

void loop()
{
    Last_Key = Key_Action(NULL);
    OLED_Diplay(NULL);
    delay(10); // 延时10ms，防止过度扫描
}

// Functions:
void OLED_Display_UI1(void *ptParam)
{
    display.clear();                   // 清除OLED屏
    display.setFont(ArialMT_Plain_16); // 设置字体大小
    display.drawString(0, 0, "Please select");
    display.drawString(0, 16, "difficulty.(0-10)");
    if (Last_Key == 11)
    {
        display.drawString(42, 32, "HELL");
    }
    else if (Last_Key < 10 && Last_Key > 0)
    {
        display.drawString(58, 32, (String)Last_Key);
    }
    display.display(); // 显示
}

void OLED_Display_UI2(void *ptParam)
{
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.drawString(8, 18, "Press * to start!");
    display.display();
    if (Last_Key == 11)
    {
        if (Game_Start == 1)
        {
            for (int i = 5; i > 0; i--)
            {
                display.clear();
                display.drawString(58, 18, (String)i);
                display.display();
                delay(1000);
            }
            User_Choice();
            display.clear();
            display.drawString(24, 0, "You Lose!");
            Display_Lose();
            display.display();
            delay(3000);
            Game_Start = 0;
            UserChoice = 0;
        }
    }
    else
    {
        if (Game_Start == 1)
        {
            for (int i = 5; i > 0; i--)
            {
                display.clear();
                display.drawString(58, 18, (String)i);
                display.display();
                delay(1000);
            }
            User_Choice();
            if (UserChoice == 0)
            {
                display.clear();
                display.drawString(28, 0, "You Lose!");
                Display_Lose();
                display.display();
                delay(3000);
            }
            else
            {
                if (esp_random() % 10 < (10 - Last_Key))
                {
                    display.clear();
                    display.drawString(26, 0, "You Win!");
                    Display_Win();
                    display.display();
                    delay(3000);
                }
                else if (esp_random() % 10 < 5)
                {
                    display.clear();
                    display.drawString(28, 0, "You Lose!");
                    Display_Lose();
                    display.display();
                    delay(3000);
                }
                else
                {
                    display.clear();
                    display.drawString(40, 0, "Draw!");
                    Display_Draw();
                    display.display();
                    delay(3000);
                }
            }
            Game_Start = 0;
            UserChoice = 0;
        }
    }
}

void OLED_Diplay(void *ptParam)
{
    if (UI_NUM == 1)
    {
        if (Fix_State == 1)
        {
            Fix_Frequence(NULL);
        } else if (Fix_State == 2)
        {
            Frequence_Set(&Rock_Frequence, 2);
        } else if (Fix_State == 3)
        {
            Frequence_Set(&Scissors_Frequence, 3);
        } else if (Fix_State == 4)
        {
            Frequence_Set(&Paper_Frequence, 4);
        } else
        {
            OLED_Display_UI1(NULL);
        }

    }
    else if (UI_NUM == 2)
    {
        OLED_Display_UI2(NULL);
    }
}

/*
TaskFrqMeter
每隔1000mS计算一次频率
*/
void Frq_Meter(void *ptParam)
{
    while (1)
    {
        FrqValue_CHN0 = (float)CountingValue_CHN0 / TimeInterval / 2;

        CountingValue_CHN0 = 0;
        vTaskDelay((unsigned int)(1000 * TimeInterval)); // 转换成 mS
    }
}

/*
TaskUART0
每隔1秒向UART0（即默认串口）发送一串字符
*/
void UART0_Printer(void *ptParam)
{
    Serial.begin(115200);

    while (1)
    {
        Serial.print("D1 Frequency:");
        Serial.println(FrqValue_CHN0);
        vTaskDelay(1000);
    }
}

void Pulse_Counting_Chn0(void)
{
    CountingValue_CHN0 = CountingValue_CHN0 + 1;
}

int Scan_Keypad(void *ptParam)
{
    for (int row = 0; row < ROW_NUM; row++)
    {
        // 将当前行设置为低电平
        digitalWrite(row_pins[row], LOW);

        for (int col = 0; col < COL_NUM; col++)
        {
            if (digitalRead(col_pins[col]) == LOW)
            {
                // 检测到按键按下
                while (digitalRead(col_pins[col]) == LOW)
                    ; // 等待按键释放
                // 恢复当前行的高电平
                digitalWrite(row_pins[row], HIGH);
                if (col != 3)
                {
                    return (row * (COL_NUM - 1) + col + 1); // 返回按键编号
                } else
                {
                    switch (row)
                    {
                    case 0:
                        return 13;
                        break;
                    case 1:
                        return 14;
                        break;
                    case 2:
                        return 15;
                        break;
                    }
                }
            }
        }

        // 恢复当前行的高电平
        digitalWrite(row_pins[row], HIGH);
    }

    return -1; // 没有按键按下
}

int Key_Action(void *ptParam)
{
    int key = Scan_Keypad(NULL);
    if (key != -1)
    {
        Serial.print("Key Pressed: ");
        Serial.println(key);
        // 在此处添加处理按键事件的代码，例如点亮LED
        switch (key)
        {
        case 10:
            if (UI_NUM == 2)
            {
                Game_Start = 1;
            }

            if (Last_Key == -1)
            {
                UI_NUM = 1;
            }
            else
            {
                UI_NUM = 2;
            }
            if (Fix_State == 1)
            {
                Fix_State = 0;
                UI_NUM = 1;
            }

            return Last_Key;
            break;
        case 12:
            if (UI_NUM == 1)
            {
                Fix_State = 1;
            }
            UI_NUM = 1;
            return Last_Key;
            break;
        case 13:
            if (Fix_State == 1)
            {
                Fix_State = 2;
            }
            return Last_Key;
            break;
        case 14:
            if (Fix_State == 1)
            {
                Fix_State = 3;
            }
            return Last_Key;
            break;
        case 15:
            if (Fix_State == 1)
            {
                Fix_State = 4;
            }
            return Last_Key;
            break;
        }
        if (UI_NUM == 1)
        {
            return key;
        }
        else
        {
            return Last_Key;
        }
    }
    return Last_Key;
}

int User_Choice()
{
    Serial.print(Rock_Frequence);
    Serial.print(" ");
    Serial.print(Scissors_Frequence);
    Serial.print(" ");
    Serial.println(Paper_Frequence);

    if (FrqValue_CHN0 > (Scissors_Frequence - ((Scissors_Frequence - Rock_Frequence) / 2)) && FrqValue_CHN0 < (Scissors_Frequence + ((Scissors_Frequence - Rock_Frequence) / 2)))
    {
        UserChoice = 2;
    }
    else if (FrqValue_CHN0 > (Rock_Frequence - ((Rock_Frequence - Paper_Frequence) / 2)) && FrqValue_CHN0 < (Rock_Frequence + ((Scissors_Frequence - Rock_Frequence) / 2)))
    {
        UserChoice = 1;
    }
    else if (FrqValue_CHN0 > (Paper_Frequence - ((Rock_Frequence - Paper_Frequence) / 2)) && FrqValue_CHN0 < (Paper_Frequence + ((Rock_Frequence - Paper_Frequence) / 2)))
    {
        UserChoice = 3;
    } else {
        UserChoice = 0;
    }
    Serial.print("User Choice.");
    Serial.println(UserChoice);
    return 0;
}

void Display_Win()
{
    if (UserChoice == 1)
    {
        display.drawIco16x16(20, 24, rock_bitmap);
        display.drawString(54, 26, "VS");
        display.drawIco16x16(92, 24, scissors_bitmap);
    }
    else if (UserChoice == 2)
    {
        display.drawIco16x16(20, 24, scissors_bitmap);
        display.drawString(54, 26, "VS");
        display.drawIco16x16(92, 24, paper_bitmap);
    }
    else if (UserChoice == 3)
    {
        display.drawIco16x16(20, 24, paper_bitmap);
        display.drawString(54, 26, "VS");
        display.drawIco16x16(92, 24, rock_bitmap);
    }
}

void Display_Lose()
{
    if (UserChoice == 0)
    {
        display.drawIco16x16(20, 24, question_mark_bitmap);
        display.drawString(54, 26, "VS");
        if (esp_random() % 3 == 0)
        {
            display.drawIco16x16(92, 24, paper_bitmap);
        }
        else if (esp_random() % 2 == 0)
        {
            display.drawIco16x16(92, 24, scissors_bitmap);
        }
        else
        {
            display.drawIco16x16(92, 24, rock_bitmap);
        }
    }
    else if (UserChoice == 1)
    {
        display.drawIco16x16(20, 24, rock_bitmap);
        display.drawString(54, 26, "VS");
        display.drawIco16x16(92, 24, paper_bitmap);
    }
    else if (UserChoice == 2)
    {
        display.drawIco16x16(20, 24, scissors_bitmap);
        display.drawString(54, 26, "VS");
        display.drawIco16x16(92, 24, rock_bitmap);
    }
    else if (UserChoice == 3)
    {
        display.drawIco16x16(20, 24, paper_bitmap);
        display.drawString(54, 26, "VS");
        display.drawIco16x16(92, 24, scissors_bitmap);
    }
}

void Display_Draw()
{
    if (UserChoice == 1)
    {
        display.drawIco16x16(20, 24, rock_bitmap);
        display.drawString(54, 26, "VS");
        display.drawIco16x16(92, 24, rock_bitmap);
    }
    else if (UserChoice == 2)
    {
        display.drawIco16x16(20, 24, scissors_bitmap);
        display.drawString(54, 26, "VS");
        display.drawIco16x16(92, 24, scissors_bitmap);
    }
    else if (UserChoice == 3)
    {
        display.drawIco16x16(20, 24, paper_bitmap);
        display.drawString(54, 26, "VS");
        display.drawIco16x16(92, 24, paper_bitmap);
    }
}

void Fix_Frequence(void *ptParam)
{
    display.clear();                   // 清除OLED屏
    display.setFont(ArialMT_Plain_16); // 设置字体大小
    display.drawString(0, 8, "Fixing Status.");
    display.display(); // 显示
}

void Frequence_Set(int *Name, int Status)
{
    int Frequence_Sum = 0; 
    for (int i = 7; i > 0; i--)
    {
        display.clear();
        display.setFont(ArialMT_Plain_16);
        switch (Status)
        {
        case 2:
            display.drawString(28, 2, "Set Rock.");
            break;
        case 3:
            display.drawString(20, 2, "Set Scissors.");
            break;
        case 4:
            display.drawString(24, 2, "Set Paper.");
            break;
        }
        display.drawString(58, 18, (String)i);
        display.display();
        delay(1000);
        if (i < 4)
        {
            Frequence_Sum += FrqValue_CHN0;
        }
    }
    *Name = Frequence_Sum / 3;
    Serial.print("Frequency Set:");
    Serial.println(*Name);
    Fix_State = 1;
}