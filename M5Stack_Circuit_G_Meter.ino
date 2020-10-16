#define M5STACK_MPU6886 
#include <M5Stack.h>

#define HISTORY 40
#define CENTER_X 135
#define CENTER_Y 120
#define ONE_G_RADIUS 80
#define MAX_G_CIRCLE 1.4
#define BG_COLOR TFT_WHITE

float acc_X = 0;
float acc_Y = 0;
float acc_Z = 0;
float ave_X = 0;
float ave_Y = 0;
float ave_Z = 0;
float sum_acc = 0;
int x[HISTORY] = {0};
int z[HISTORY] = {0};
void setup(){
  M5.begin();
  M5.Power.begin();

  M5.IMU.Init();

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(BG_COLOR , BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextFont(4);

  // Speaker Noise Reduce
  M5.Speaker.mute();
  pinMode(25, OUTPUT);
  digitalWrite(25, LOW);

  // Initialize Arr
    for(int i=0;i<HISTORY;i++)
  {
    x[i] = CENTER_X;
    z[i] = CENTER_Y;
  }

  // Strartup
  M5.Lcd.drawRect(30, 50, 260, 110, TFT_CYAN);
  M5.Lcd.setTextColor(TFT_CYAN);
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawString("G Meter", 50, 70);
  M5.Lcd.setTextSize(1);
  M5.Lcd.drawString("For Racing", 150, 120);
  M5.Lcd.setTextColor(TFT_BLUE);
  M5.Lcd.drawString("designed by yukari", 100, 210);
  delay(3000);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextColor(BG_COLOR);
  delay(500);
}

void loop() {
  M5.IMU.getAccelData(&acc_X,&acc_Y,&acc_Z);
  for(int i=HISTORY-1;i>0;i--)
  {
    x[i] = x[i-1];
    z[i] = z[i-1];
  }
  x[0] = acc_X * -1 * ONE_G_RADIUS + CENTER_X;
  z[0] = -acc_Z * ONE_G_RADIUS + CENTER_Y;

  // draw bg
  M5.Lcd.setTextSize(1);
  M5.Lcd.drawFastHLine(20, CENTER_Y, 320, BG_COLOR);
  M5.Lcd.drawFastVLine(CENTER_X, 0, 240, BG_COLOR);
  M5.Lcd.drawString("1",CENTER_X+80,CENTER_Y+2);
  M5.Lcd.drawString(String(MAX_G_CIRCLE,1),CENTER_X+113,CENTER_Y+2); 
  M5.Lcd.drawCircle(CENTER_X, CENTER_Y, ONE_G_RADIUS, BG_COLOR);
  M5.Lcd.drawCircle(CENTER_X, CENTER_Y, ONE_G_RADIUS*MAX_G_CIRCLE, BG_COLOR);

  // draw G circle & history 
  for(int i=1;i<HISTORY;i++)
  {
    M5.Lcd.fillCircle(x[i], z[i], 4, TFT_ORANGE);
    M5.Lcd.drawLine(x[i], z[i], x[i-1], z[i-1], TFT_ORANGE);
  }
  M5.Lcd.fillCircle(x[HISTORY-1], z[HISTORY-1], 4, TFT_BLACK);
  M5.Lcd.drawLine(x[HISTORY-1], z[HISTORY-1], x[HISTORY-2], z[HISTORY-2], TFT_BLACK);
  M5.Lcd.fillCircle(x[1], z[1], 20, TFT_BLACK);
  M5.Lcd.fillCircle(x[0], z[0], 20, TFT_ORANGE);
  
  // draw G num
  sum_acc = sum_acc * 0.9 + sqrt(acc_X*acc_X + acc_Z*acc_Z) * 0.1;
  M5.Lcd.setTextSize(2);
  M5.Lcd.fillRect(250, 0, 90, 40, TFT_BLACK);
  M5.Lcd.drawString(String(abs(sum_acc),1), 250, 0);
  M5.Lcd.setTextSize(1);
  M5.Lcd.drawString("G",300,50);

  // draw G bar
  float acc_X_limited = constrain(acc_X,-MAX_G_CIRCLE,MAX_G_CIRCLE);
  int x_start = CENTER_X - ONE_G_RADIUS * MAX_G_CIRCLE;
  int x_width = ONE_G_RADIUS * MAX_G_CIRCLE * 2;
  int x_bar = ONE_G_RADIUS * (-acc_X_limited + MAX_G_CIRCLE);
  M5.Lcd.fillRect(x_start, 220, x_bar, 20, TFT_GREEN);
  M5.Lcd.fillRect(x_start+x_bar, 220, x_width-x_bar, 20, TFT_BLACK);
  M5.Lcd.drawRect(x_start, 220, x_width, 20, BG_COLOR);
  float acc_Z_limited = constrain(acc_Z,-MAX_G_CIRCLE,MAX_G_CIRCLE);
  int y_start = CENTER_Y - ONE_G_RADIUS * MAX_G_CIRCLE;
  int y_width = ONE_G_RADIUS * MAX_G_CIRCLE * 2;
  int y_bar = ONE_G_RADIUS * (-acc_Z_limited + MAX_G_CIRCLE);
  M5.Lcd.fillRect(0, y_start, 20, y_bar, TFT_BLACK);
  M5.Lcd.fillRect(0, y_start+y_bar, 20, y_width-y_bar, TFT_GREEN);
  M5.Lcd.drawRect(0, y_start, 20, y_width, BG_COLOR);
  
  //delay(100);
}
