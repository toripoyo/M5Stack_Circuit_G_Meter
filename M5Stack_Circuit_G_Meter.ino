// -----------------------------------------------------------------------------
//  M5Unified-based G-Meter (Core Gray)
//  - Zero‑angle calibration (BtnB) is stored in EEPROM and restored on boot
//  - Works with the latest M5Unified + M5GFX libraries (May 2025)
// -----------------------------------------------------------------------------
#include <M5Unified.h>
#include <EEPROM.h>
#include <array>      // std::array
#include <algorithm>  // std::rotate, std::max

// ─────────────────────────────────────────────────────────────────────────────
//  Constants
// ─────────────────────────────────────────────────────────────────────────────
constexpr int G_HISTORY_NUM = 50;
constexpr uint16_t BG_COLOR = TFT_WHITE;
constexpr uint8_t kCircleCenterPosX = 160;
constexpr uint8_t kCircleCenterPosY = 120;
constexpr uint8_t kOneGRadius = 100;           // pixel radius representing 1 G
constexpr float kMaxGValue = 1.2f;             // draw up to ±1.2 G
constexpr float kMaxGValueAcc = 0.6f;          // Accelearion Max 0.4G
constexpr float kGFilteringCoeff = 0.8f;       // exponential moving‑average coeff
constexpr uint32_t EEPROM_MAGIC = 0xA5A5A5A5;  // identifies valid data

// ─────────────────────────────────────────────────────────────────────────────
//  Types / Globals
// ─────────────────────────────────────────────────────────────────────────────
struct PixelPos {
  uint16_t x, y;
};
struct AccelVect {
  float x, y, z;
};
struct CalibData {
  uint32_t magic;
  float theta;
};

bool g_initialized = false;   // true once we have a valid θ offset
float g_theta = 0.0f;         // roll‑axis calibration angle [rad]
M5Canvas g_buf(&M5.Display);  // off‑screen buffer for flicker‑free rendering

// G‑history using std::array for rotate
std::array<uint16_t, G_HISTORY_NUM> hist_x{};
std::array<uint16_t, G_HISTORY_NUM> hist_y{};

// ─────────────────────────────────────────────────────────────────────────────
//  EEPROM helpers
// ─────────────────────────────────────────────────────────────────────────────
static void loadCalibration() {
  CalibData c{};
  EEPROM.get(0, c);
  if (c.magic == EEPROM_MAGIC && std::isfinite(c.theta)) {
    g_theta = c.theta;
    g_initialized = true;
  }
}
static void saveCalibration() {
  CalibData c{ EEPROM_MAGIC, g_theta };
  EEPROM.put(0, c);
  EEPROM.commit();
  delay(500);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Math helpers
// ─────────────────────────────────────────────────────────────────────────────
static float calcAccelSum(const AccelVect &a) {
  return std::max(0.0f, hypotf(a.x, a.z) - 0.04f);
}

static AccelVect correctAccelOffset(const AccelVect &in) {
  AccelVect v = in;

  // Capture new offset when BtnB pressed (or at first run)
  if (!g_initialized || M5.BtnB.isPressed()) {
    g_theta = -atan2f(v.z, v.y);
    g_initialized = true;
    saveCalibration();
  }

  // Rotate so +Z aligns with gravity when vehicle is level
  const float cz = cosf(g_theta);
  const float sz = sinf(g_theta);
  float z_new = v.z * cz + v.y * sz;
  float y_new = v.z * sz + v.y * cz;
  v.z = z_new;
  v.y = y_new;

  // Clamp values
  v.x = constrain(v.x, -kMaxGValue, kMaxGValue);
  v.y = constrain(v.y, -kMaxGValue, kMaxGValue);
  v.z = constrain(v.z, -kMaxGValueAcc, kMaxGValue);

  return v;
}

static AccelVect averageAccel(const AccelVect &in) {
  static AccelVect v{ 0, 0, 0 };
  v.x = v.x * kGFilteringCoeff + in.x * (1.0f - kGFilteringCoeff);
  v.y = v.y * kGFilteringCoeff + in.y * (1.0f - kGFilteringCoeff);
  v.z = v.z * kGFilteringCoeff + in.z * (1.0f - kGFilteringCoeff);
  return v;
}

static PixelPos convertAccel2PixelPos(const AccelVect &a) {
  PixelPos pos = { 0, 0 };
  pos.x = -a.x * kOneGRadius + kCircleCenterPosX;
  if (-a.z < 0) {
    pos.y = -a.z * kOneGRadius + kCircleCenterPosY;
  } else {
    pos.y = -a.z / kMaxGValueAcc * kOneGRadius + kCircleCenterPosY;  // 0.4G Max
  }

  return pos;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Arduino setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  auto cfg = M5.config();   // Default config is fine for Core & Core2
  cfg.output_power = true;  // enable AXP192 / IP5306 control
  cfg.internal_spk = true;
  M5.begin(cfg);
  M5.Speaker.begin();
  M5.Speaker.stop();

  if (!M5.Imu.begin()) {
    M5.Display.println("IMU not found!");
    delay(1000);
  }

  EEPROM.begin(1024);
  loadCalibration();

  // Prepare double buffer (8‑bit)
  g_buf.setColorDepth(8);
  g_buf.createSprite(320, 240);
  g_buf.setTextColor(BG_COLOR, TFT_BLACK);
  g_buf.setTextSize(2);
  g_buf.setFont(&fonts::Font4);
  M5.Display.setBrightness(255);

  // ── Startup splash (2 s) ────────────────────────────────────────
  g_buf.fillSprite(TFT_BLACK);
  g_buf.drawRect(30, 50, 260, 110, TFT_CYAN);
  g_buf.setTextColor(TFT_CYAN);
  g_buf.drawString("G Meter", 50, 70);
  g_buf.setTextSize(1);
  g_buf.drawString("For Racing", 150, 120);
  g_buf.setTextColor(TFT_BLUE);
  g_buf.pushSprite(0, 0);
  delay(2000);

  // Clear screen
  g_buf.fillSprite(TFT_BLACK);
  g_buf.setTextColor(BG_COLOR);
  g_buf.pushSprite(0, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Arduino loop
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  M5.update();

  // ── Acquire & process accelerometer data ────────────────────────
  AccelVect acc;
  M5.Imu.getAccelData(&acc.x, &acc.y, &acc.z);
  acc = correctAccelOffset(acc);
  acc = averageAccel(acc);

  // ── Clear frame ─────────────────────────────────────────────────
  g_buf.fillSprite(TFT_BLACK);

  // ── Draw grid & guides ──────────────────────────────────────────
  g_buf.drawFastHLine(20, kCircleCenterPosY, 320, BG_COLOR);
  g_buf.drawFastVLine(kCircleCenterPosX, 0, 240, BG_COLOR);
  g_buf.setTextSize(1);
  g_buf.drawString("1.0", 270, 218);
  g_buf.drawString("1.0", 22, 20);
  g_buf.drawString(String(kMaxGValueAcc, 1), 22, 197);
  g_buf.drawCircle(kCircleCenterPosX, kCircleCenterPosY, kOneGRadius, BG_COLOR);

  // ── History trail ───────────────────────────────────────────────
  PixelPos nowPix = convertAccel2PixelPos(acc);
  std::rotate(hist_x.rbegin(), hist_x.rbegin() + 1, hist_x.rend());
  std::rotate(hist_y.rbegin(), hist_y.rbegin() + 1, hist_y.rend());
  hist_x[0] = nowPix.x;
  hist_y[0] = nowPix.y;

  // Draw trajectory
  for (size_t i = 1; i < hist_x.size(); ++i) {
    g_buf.fillCircle(hist_x[i], hist_y[i], 4, TFT_ORANGE);
    g_buf.drawLine(hist_x[i], hist_y[i], hist_x[i - 1], hist_y[i - 1], TFT_ORANGE);
  }

  // ── Current G‑vector ────────────────────────────────────────────
  g_buf.fillCircle(nowPix.x, nowPix.y, 15, TFT_GREEN);
  g_buf.setTextSize(2);
  g_buf.drawString(String(calcAccelSum(acc), 1), 250, 0);
  g_buf.setTextSize(1);
  g_buf.drawString("G", 300, 50);

  // ── Symmetric X‑axis bar (horizontal, bottom) ──────────────────
  const int16_t barHeight = 20;
  const int16_t x_center = kCircleCenterPosX;
  const int16_t x_half = kOneGRadius;  // max ± length
  const int16_t baselineY = 220;
  const int16_t x_len = nowPix.x - kCircleCenterPosX;

  g_buf.drawRect(x_center - x_half, baselineY, x_half * 2, barHeight, BG_COLOR);
  if (x_len >= 0) {
    g_buf.fillRect(x_center, baselineY, x_len, barHeight, TFT_GREEN);
  } else {
    g_buf.fillRect(x_center + x_len, baselineY, -x_len, barHeight, TFT_GREEN);
  }

  // ── Symmetric Y‑axis bar (vertical, left) ───────────────────────
  const int16_t y_center = kCircleCenterPosY;
  const int16_t y_half = kOneGRadius;
  const int16_t barWidth = 20;
  const int16_t y_len = nowPix.y - kCircleCenterPosY;

  g_buf.drawRect(0, y_center - y_half, barWidth, y_half * 2, BG_COLOR);
  if (y_len >= 0) {
    g_buf.fillRect(0, y_center + y_len, barWidth, -y_len, TFT_GREEN);  // Accel
  } else {
    g_buf.fillRect(0, y_center, barWidth, y_len, TFT_RED);  // Brake
  }

  // ── Present frame ───────────────────────────────────────────────
  g_buf.pushSprite(0, 0);
}
