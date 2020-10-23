#define M5STACK_MPU6886
#include <M5Stack.h>
#include "EEPROM.h"

// Const
#define G_HISTORY_NUM 50
#define BG_COLOR TFT_WHITE
const uint8_t kCircleCenterPosX = 135;
const uint8_t kCircleCenterPosY = 120;
const uint8_t kOneGRadius = 80;
const float kMaxGValue = 1.4;
const uint16_t kZeroToneFreq = 1500;
const float kGFilteringCoeff = 0.5;

// Global
bool g_enable_sound = false;

// Display Double Buffer
TFT_eSprite g_TFTBuf = TFT_eSprite(&M5.Lcd);

// Struct
struct PixelPos_t
{
  uint16_t x, y;
};

struct AccelVect_t
{
  float x, y, z;
};

// *******************************************************

void setup()
{
  // System Init
  M5.begin();
  M5.Power.begin();
  M5.IMU.Init();
  EEPROM.begin(1024);
  delay(250);
  M5.Lcd.fillScreen(TFT_BLACK);
  g_TFTBuf.setColorDepth(8);
  g_TFTBuf.createSprite(320, 240);
  g_TFTBuf.setTextColor(BG_COLOR, TFT_BLACK);
  g_TFTBuf.setTextSize(2);
  g_TFTBuf.setTextFont(4);

  // Show Strartup Scr.
  g_TFTBuf.fillSprite(TFT_BLACK);
  g_TFTBuf.drawRect(30, 50, 260, 110, TFT_CYAN);
  g_TFTBuf.setTextColor(TFT_CYAN);
  g_TFTBuf.setTextSize(2);
  g_TFTBuf.drawString("G Meter", 50, 70);
  g_TFTBuf.setTextSize(1);
  g_TFTBuf.drawString("For Racing", 150, 120);
  g_TFTBuf.setTextColor(TFT_BLUE);
  g_TFTBuf.drawString("Press Left to Track Mode", 0, 200);
  g_TFTBuf.pushSprite(0, 0);
  delay(2000);

  // Detecct Button
  M5.update();
  if (M5.BtnA.isPressed())
  {
    g_enable_sound = true;
  }

  // Speaker Noise Reduce
  if(g_enable_sound)
  {
    M5.Speaker.begin();
    M5.Speaker.tone(kZeroToneFreq, 50);delay(200);
    M5.Speaker.mute();
    M5.Speaker.tone(kZeroToneFreq, 50);delay(200);
    M5.Speaker.mute();
  }else{
    M5.Speaker.mute();
    pinMode(25, OUTPUT);
    digitalWrite(25, LOW);
  }

  g_TFTBuf.fillSprite(TFT_BLACK);
  g_TFTBuf.setTextColor(BG_COLOR);
  g_TFTBuf.pushSprite(0, 0);
  delay(500);
}

// Calc Sum of Accel
float calcAccelVectSum(AccelVect_t inputAccel)
{
  float internal = sqrt(inputAccel.x * inputAccel.x + inputAccel.z * inputAccel.z);
  internal -= 0.04;
  if(internal <= 0)internal = 0;
  return internal; 
}

// Correct Accel Offset
AccelVect_t correctAccelOffset(AccelVect_t inputAccel)
{
  AccelVect_t internal;
  static float theta = 0;

  M5.update();
  if (M5.BtnB.isPressed())
  {
    theta = -atan2(inputAccel.z, inputAccel.y);
  }

  internal.x = constrain(inputAccel.x, -kMaxGValue, kMaxGValue);
  internal.y = constrain(inputAccel.y, -kMaxGValue, kMaxGValue);
  internal.z = constrain(inputAccel.z, -kMaxGValue, kMaxGValue);
  
  internal.z = internal.z * cos(theta) + internal.y * sin(theta);
  internal.y = internal.z * sin(theta) + internal.y * cos(theta);
  return internal;
}

// Averaging Accel
AccelVect_t averageAccel(AccelVect_t inputAccel)
{
  static AccelVect_t internal = {0,0,0};

  internal.x = internal.x * kGFilteringCoeff + inputAccel.x * (1.0-kGFilteringCoeff);
  internal.y = internal.y * kGFilteringCoeff + inputAccel.y * (1.0-kGFilteringCoeff);
  internal.z = internal.z * kGFilteringCoeff + inputAccel.z * (1.0-kGFilteringCoeff);
  return internal;
}

// Get Pixel Position from Accel
PixelPos_t convertAccel2PixelPos(AccelVect_t inputAccel)
{
  struct PixelPos_t pix;
  pix.x = -inputAccel.x * kOneGRadius + kCircleCenterPosX;
  pix.y = -inputAccel.z * kOneGRadius + kCircleCenterPosY;
  return pix;
}

// Get Color from Accel
uint16_t getColorFromAccel(float accVal)
{
  // Decide Color from now Accel(Green -> Red)
  float accVal_limited = (abs(accVal) >= 1.0)?1.0:abs(accVal);
  uint16_t red_level = 255.0 * accVal_limited;
  uint16_t green_level = 255.0 * (1.0 - accVal_limited);
  return M5.Lcd.color565(red_level,green_level,0);
}

void loop()
{
  static uint16_t hist_x[G_HISTORY_NUM] = {kCircleCenterPosX};
  static uint16_t hist_y[G_HISTORY_NUM] = {kCircleCenterPosY};
  static uint8_t nowHistBufPos = 0;
  static struct PixelPos_t nowPixPos;
  static struct AccelVect_t nowAccelVect;
  static uint64_t prev_micros = 0;
  static uint16_t sound_interval_count = 0;

  g_TFTBuf.fillSprite(TFT_BLACK);

  // Calc Flame Rate
  uint64_t now_micros = micros();
  uint16_t now_fps = (uint16_t)(1000000.0/((float)now_micros - (float)prev_micros));
  prev_micros = now_micros;
  g_TFTBuf.drawString(String(now_fps), 290, 215);
  
  // Get Accel Data
  M5.IMU.getAccelData(&nowAccelVect.x, &nowAccelVect.y, &nowAccelVect.z);
  nowAccelVect = correctAccelOffset(nowAccelVect);
  nowAccelVect = averageAccel(nowAccelVect);

  // Play Tone
  if(g_enable_sound)
  {
    sound_interval_count++;
    if(sound_interval_count >= 2)
    {
      sound_interval_count = 0;
      uint16_t tone_freq = kZeroToneFreq + kZeroToneFreq * calcAccelVectSum(nowAccelVect);
      M5.Speaker.tone(tone_freq, 10);  // duration is dummy
    }else{
      M5.Speaker.mute();
    }
  }

  // Draw Background
  g_TFTBuf.setTextSize(1);
  g_TFTBuf.drawFastHLine(20, kCircleCenterPosY, 320, BG_COLOR);
  g_TFTBuf.drawFastVLine(kCircleCenterPosX, 0, 240, BG_COLOR);
  g_TFTBuf.drawString("1", kCircleCenterPosX + 80, kCircleCenterPosY + 2);
  g_TFTBuf.drawString(String(kMaxGValue, 1), kCircleCenterPosX + 113, kCircleCenterPosY + 2);
  g_TFTBuf.drawCircle(kCircleCenterPosX, kCircleCenterPosY, kOneGRadius, BG_COLOR);
  g_TFTBuf.drawCircle(kCircleCenterPosX, kCircleCenterPosY, kOneGRadius * kMaxGValue, BG_COLOR);

  // draw G History
  nowPixPos = convertAccel2PixelPos(nowAccelVect);
  for(int i=G_HISTORY_NUM-1;i>0;i--)
  {
    hist_x[i] = hist_x[i-1];
    hist_y[i] = hist_y[i-1];
  }
  hist_x[0] = nowPixPos.x;
  hist_y[0] = nowPixPos.y;
  for(int i=1;i<G_HISTORY_NUM;i++)
  {
    g_TFTBuf.fillCircle(hist_x[i], hist_y[i], 4, TFT_ORANGE);
    g_TFTBuf.drawLine(hist_x[i], hist_y[i], hist_x[i-1], hist_y[i-1], TFT_ORANGE);
  }

  // Draw G Point
  uint16_t circle_color = getColorFromAccel(calcAccelVectSum(nowAccelVect));
  g_TFTBuf.fillCircle(nowPixPos.x, nowPixPos.y, 20, circle_color);

  // Draw G Value
  g_TFTBuf.setTextSize(2);
  g_TFTBuf.drawString(String(calcAccelVectSum(nowAccelVect), 1), 250, 0);
  g_TFTBuf.setTextSize(1);
  g_TFTBuf.drawString("G", 300, 50);
  
  // draw G bar(X)
  int x_start = kCircleCenterPosX - kOneGRadius * kMaxGValue;
  int x_width = kOneGRadius * kMaxGValue * 2;
  int x_bar = kOneGRadius * (-nowAccelVect.x + kMaxGValue);
  uint16_t barX_color = getColorFromAccel(nowAccelVect.x);
  g_TFTBuf.fillRect(x_start, 220, x_bar, 20, barX_color);
  g_TFTBuf.fillRect(x_start + x_bar, 220, x_width - x_bar, 20, TFT_BLACK);
  g_TFTBuf.drawRect(x_start, 220, x_width, 20, BG_COLOR);
  // draw G bar(Y)
  int y_start = kCircleCenterPosY - kOneGRadius * kMaxGValue;
  int y_width = kOneGRadius * kMaxGValue * 2;
  int y_bar = kOneGRadius * (-nowAccelVect.z + kMaxGValue);
  uint16_t barY_color = getColorFromAccel(nowAccelVect.z);
  g_TFTBuf.fillRect(0, y_start, 20, y_bar, TFT_BLACK);
  g_TFTBuf.fillRect(0, y_start + y_bar, 20, y_width - y_bar, barY_color);
  g_TFTBuf.drawRect(0, y_start, 20, y_width, BG_COLOR);

  g_TFTBuf.pushSprite(0, 0);
}
