#include <Wire.h>
#include <MPU6050_tockn.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>
#include "DFRobotDFPlayerMini.h" 

#define LED_PIN     23   // WS2812灯带引脚
#define NUM_LEDS    16
#define FSR_1       34
#define FSR_2       35
#define FSR_3       32
#define SERVO_PIN   18
#define HEAT_PIN    13
#define ASR_RX      17 //pb6
#define ASR_TX      16 //pb5
#define MP3_TX      25
#define MP3_RX      26
#define BUSY_PIN    33

Adafruit_NeoPixel ring(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
MPU6050 mpu6050(Wire);
Servo tail;
DFRobotDFPlayerMini myDFPlayer;

unsigned long lastHeatStart = 0;
bool heating = false;
bool hasBeenLifted = false;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  mpu6050.begin();
  mpu6050.calcGyroOffsets(true);

  pinMode(FSR_1, INPUT);
  pinMode(FSR_2, INPUT);
  pinMode(FSR_3, INPUT);
  pinMode(HEAT_PIN, OUTPUT);
  digitalWrite(HEAT_PIN, LOW);
  pinMode(BUSY_PIN, INPUT); // 将BUSY引脚设置为输入

  tail.attach(SERVO_PIN);
  ring.begin();
  ring.show();

  Serial2.begin(9600, SERIAL_8N1, ASR_RX, ASR_TX);  // 语音识别模块
  // 初始化MP3模块
  Serial1.begin(9600, SERIAL_8N1, MP3_RX, MP3_TX); // 初始化MP3模块的串口
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

  if (!myDFPlayer.begin(Serial1)) {
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the TF card!"));
    // 如果初始化失败，可以在这里停止程序或进行其他处理
    // while(true); 
  }
  Serial.println(F("DFPlayer Mini online."));
  myDFPlayer.volume(20);  // 设置音量 (0~30)

  delay(500);


}

void loop() {
  mpu6050.update();
  checkLifted();
  handleFSRs();
  handleASR();
  manageHeating();
  // Serial.println(digitalRead(BUSY_PIN));  // LOW = 正在播放，HIGH = 没有播放
  delay(100);

  // ----------- 调试用：打印 MPU6050 Y轴加速度 -----------
  // static unsigned long lastPrint = 0;
  // if (millis() - lastPrint > 1000) {
  //   lastPrint = millis();
  //   Serial.print("AccZ = ");
  //   Serial.println(mpu6050.getAccZ());
  // }
}

// 播放指定MP3文件的函数
void playMP3(int trackNumber) {
  // 检查当前是否正在播放，如果正在播放则不打断
  if (digitalRead(BUSY_PIN) == HIGH) { // HIGH表示空闲
    Serial.print("Playing track: ");
    Serial.println(trackNumber);
    myDFPlayer.play(trackNumber);
  } else {
    Serial.println("MP3 is busy, skipping new track request.");
  }
}

// ---------------- MPU6050 -----------------
void checkLifted() {
  float accZ = mpu6050.getAccZ();

  if (!hasBeenLifted && accZ < 0.5) {  // 被抬起：Z轴加速度显著小于1g
    hasBeenLifted = true;
    Serial.println("🐱 被平稳抬起！");
    wagTail();
  } else if (accZ >= 0.95) {  // 放回桌上
    hasBeenLifted = false;
  }
}

// ---------------- SG90 尾巴 -----------------
void wagTail() {
  for (int i = 0; i < 3; i++) {
    tail.write(60);
    delay(300);
    tail.write(120);
    delay(300);
  }
  tail.write(90);
}

// ---------------- FSR -----------------
void handleFSRs() {
  int f1 = analogRead(FSR_1);
  int f2 = analogRead(FSR_2);
  int f3 = analogRead(FSR_3);
  int total = f1 + f2 + f3;

  static unsigned long touchStart = 0;
  static bool inLongTouch = false;

  static int touchCount = 0;
  static unsigned long firstTouchTime = 0;

  const int TOUCH_THRESHOLD = 1000;
  const int MULTI_TOUCH_COUNT = 3;
  const int MULTI_TOUCH_WINDOW = 3000; // 3秒内多次触摸

  static bool wasTouching = false;

  bool isTouching = (total > TOUCH_THRESHOLD);

  if (total > 1000) {
    if (touchStart == 0) touchStart = millis();
    if (millis() - touchStart > 6000) {
      if (!inLongTouch) {
        inLongTouch = true;
        for (int i = 0; i < 3; i++) {
          breatheColor(255, 255, 0);  // 黄色
        }
      }
    } else {
      if (!inLongTouch) {
        for (int i = 0; i < 3; i++) {
          breatheColor(255, 255, 255);  // 白色
        }
      }
    }
  } else {
    touchStart = 0;
    inLongTouch = false;
  }
}

// ---------------- 灯效 -----------------
void breatheColor(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i <= 255; i += 2) {
    setRingBrightness(i, r, g, b);
    delay(20);
  }
  for (int i = 255; i >= 0; i -= 2) {
    setRingBrightness(i, r, g, b);
    delay(20);
  }
}

void setRingBrightness(uint8_t brightness, uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUM_LEDS; i++) {
    ring.setPixelColor(i, ring.Color(r * brightness / 255, g * brightness / 255, b * brightness / 255));
  }
  ring.show();
}

// ---------------- ASR 语音识别 -----------------
void handleASR() {
  if (Serial2.available()) {
    String cmd = Serial2.readStringUntil('\n');
    cmd.trim();
    Serial.println("识别到命令：" + cmd);
    // Serial.println(cmd);

    if (cmd == "dog") {//如果是狗狗就替换成dog
      // int r = random(3);
      // if (r == 0) breatheColor(255, 255, 255);  // 白色
      // else if (r == 1) wagTail();
      else if (r == 2) playMP3(1);
    } else if (cmd == "happy") {
      delay(1000);  // 等主人说完
      breatheColor(255, 255, 0);  // 黄色
    } else if (cmd == "sad") {
      delay(1000);
      breatheColor(100, 100, 255);  // 淡蓝色
    }
  }
}


// ---------------- 石墨烯加热 -----------------
void manageHeating() {
  if (!heating && millis() - lastHeatStart > 30000UL) {  // 每半分钟启动一次
    heating = true;
    digitalWrite(HEAT_PIN, HIGH);
    lastHeatStart = millis();
  } else if (heating && millis() - lastHeatStart > 30000UL) {  // 加热半分钟
    digitalWrite(HEAT_PIN, LOW);
    heating = false;
  }
}