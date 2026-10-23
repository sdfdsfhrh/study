/*
 * IoT SECURITY PROJECT Rayder - 최종본
 * 보드: ESP32 DevKitC 38핀 (USB-C)
 * 기능: 보안 모드(침입 감지) + 공부 도우미 모드(착석 시간 측정)
 * 필요한 라이브러리: UniversalTelegramBot, ArduinoJson
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// ===== 여기만 본인 정보로 바꾸기 =====
const char* WIFI_SSID = "와이파이이름";
const char* WIFI_PASS = "와이파이비밀번호";
#define BOT_TOKEN "텔레그램_봇_토큰"
#define CHAT_ID   "내_채팅_ID"

// ===== 핀 번호 =====
const int PIR_PIN    = 13;  // PIR 센서 OUT
const int BUZZER_PIN = 25;  // 능동 부저 +
const int GREEN_LED  = 26;  // 초록 LED (저항 거쳐서)
const int RED_LED    = 27;  // 빨간 LED (저항 거쳐서)

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// ===== 상태 변수 =====
enum Mode { SECURITY, STUDY };
Mode mode = SECURITY;      // 처음엔 보안 모드
bool systemOn = true;      // /off 하면 false
bool alarmOn = false;      // 경보 울리는 중인지

// 시간 관련 (delay 대신 millis로 관리)
unsigned long lastBotCheck = 0;
const unsigned long BOT_INTERVAL = 1000;      // 1초마다 텔레그램 확인
unsigned long lastAlertTime = 0;
const unsigned long ALERT_COOLDOWN = 30000;   // 알림은 30초에 한 번만
const unsigned long ALARM_DURATION = 60000;   // 경보는 최대 1분 후 자동 종료
unsigned long alarmStartTime = 0;
unsigned long lastBeepToggle = 0;
bool buzzerState = false;

// 공부 도우미 모드용
unsigned long goalMinutes = 0;      // 목표 시간(분)
unsigned long studyMillis = 0;      // 누적 공부 시간(ms)
unsigned long lastMotionTime = 0;   // 마지막으로 움직임 감지된 시각
unsigned long lastStudyTick = 0;
bool goalDone = false;
const unsigned long AWAY_TIMEOUT = 60000;  // 1분 동안 움직임 없으면 자리 비움

// ===== 작은 도우미 함수들 =====
void setLeds(bool green, bool red) {
  digitalWrite(GREEN_LED, green ? HIGH : LOW);
  digitalWrite(RED_LED, red ? HIGH : LOW);
}

void stopAlarm() {
  alarmOn = false;
  buzzerState = false;
  digitalWrite(BUZZER_PIN, LOW);
}

void beep(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(150);
    digitalWrite(BUZZER_PIN, LOW);
    delay(150);
  }
}

String statusText() {
  String s = "📊 현재 상태\n";
  s += "전원: " + String(systemOn ? "켜짐" : "꺼짐") + "\n";
  s += "모드: " + String(mode == SECURITY ? "보안 모드" : "공부 도우미 모드") + "\n";
  if (mode == SECURITY) {
    s += "경보: " + String(alarmOn ? "울리는 중" : "없음") + "\n";
  } else {
    s += "공부 시간: " + String(studyMillis / 60000) + "분\n";
    if (goalMinutes > 0) s += "목표: " + String(goalMinutes) + "분\n";
  }
  return s;
}

String helpText() {
  return "📌 명령어 목록\n"
         "/status - 현재 상태 보기\n"
         "/on - 시스템 켜기\n"
         "/off - 시스템 끄기\n"
         "/alarm_off - 경보 끄기\n"
         "/security - 보안 모드로 전환\n"
         "/study - 공부 도우미 모드로 전환\n"
         "/goal 60 - 목표 공부 시간 설정(분)";
}

// ===== 텔레그램 명령어 처리 =====
void handleMessages(int count) {
  for (int i = 0; i < count; i++) {
    String chatId = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    // 내 채팅방이 아니면 무시
    if (chatId != CHAT_ID) {
      bot.sendMessage(chatId, "권한이 없는 사용자입니다.", "");
      continue;
    }

    if (text == "/start" || text == "/help") {
      bot.sendMessage(CHAT_ID, helpText(), "");
    }
    else if (text == "/status") {
      bot.sendMessage(CHAT_ID, statusText(), "");
    }
    else if (text == "/on") {
      systemOn = true;
      bot.sendMessage(CHAT_ID, "✅ 시스템을 켰습니다.", "");
    }
    else if (text == "/off") {
      systemOn = false;
      stopAlarm();
      setLeds(false, false);
      bot.sendMessage(CHAT_ID, "⏹ 시스템을 껐습니다.", "");
    }
    else if (text == "/alarm_off") {
      stopAlarm();
      bot.sendMessage(CHAT_ID, "🔕 경보를 껐습니다.", "");
    }
    else if (text == "/security") {
      mode = SECURITY;
      stopAlarm();
      bot.sendMessage(CHAT_ID, "🛡 보안 모드로 전환했습니다.", "");
    }
    else if (text == "/study") {
      mode = STUDY;
      stopAlarm();
      studyMillis = 0;
      goalDone = false;
      lastMotionTime = 0;
      bot.sendMessage(CHAT_ID, "📚 공부 도우미 모드로 전환했습니다.\n/goal 분 으로 목표를 정해 보세요.", "");
    }
    else if (text.startsWith("/goal")) {
      int minutes = text.substring(5).toInt();  // "/goal 60" → 60
      if (minutes > 0) {
        goalMinutes = minutes;
        studyMillis = 0;
        goalDone = false;
        bot.sendMessage(CHAT_ID, "🎯 목표 " + String(minutes) + "분 설정 완료!", "");
      } else {
        bot.sendMessage(CHAT_ID, "사용법: /goal 60  (숫자는 분)", "");
      }
    }
    else {
      bot.sendMessage(CHAT_ID, "모르는 명령어예요.\n" + helpText(), "");
    }
  }
}

// ===== 보안 모드 =====
void runSecurity(bool motion, unsigned long now) {
  // 움직임 감지 + 경보 중이 아님 + 쿨다운 지남 → 경보 시작
  if (motion && !alarmOn &&
      (lastAlertTime == 0 || now - lastAlertTime > ALERT_COOLDOWN)) {
    alarmOn = true;
    alarmStartTime = now;
    lastAlertTime = now;
    bot.sendMessage(CHAT_ID, "🚨 침입 감지! 움직임이 포착되었습니다.\n/alarm_off 로 경보를 끌 수 있어요.", "");
  }

  if (alarmOn) {
    setLeds(false, true);  // 빨간 LED
    // 부저를 0.3초마다 켰다 껐다 (삐-삐-삐)
    if (now - lastBeepToggle > 300) {
      buzzerState = !buzzerState;
      digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
      lastBeepToggle = now;
    }
    // 1분 지나면 자동으로 끄기
    if (now - alarmStartTime > ALARM_DURATION) stopAlarm();
  } else {
    setLeds(true, false);  // 평소엔 초록 LED
  }
}

// ===== 공부 도우미 모드 =====
void runStudy(bool motion, unsigned long now) {
  if (motion) lastMotionTime = now;

  // 최근 1분 안에 움직임이 있었으면 "앉아 있음"
  bool present = (lastMotionTime != 0) && (now - lastMotionTime < AWAY_TIMEOUT);

  if (present) studyMillis += now - lastStudyTick;
  setLeds(present, !present);  // 앉아 있으면 초록, 자리 비우면 빨강

  // 목표 달성 체크
  if (goalMinutes > 0 && !goalDone && studyMillis >= goalMinutes * 60000UL) {
    goalDone = true;
    beep(3);
    bot.sendMessage(CHAT_ID, "🎉 목표 " + String(goalMinutes) + "분 달성! 수고했어요, 잠깐 쉬어도 좋아요!", "");
  }
}

// ===== setup =====
void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  setLeds(false, false);
  digitalWrite(BUZZER_PIN, LOW);

  // 와이파이 연결 (연결될 때까지 초록 LED 깜빡임)
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  Serial.print("WiFi 연결 중");
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(GREEN_LED, !digitalRead(GREEN_LED));
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi 연결 완료: " + WiFi.localIP().toString());

  // PIR 센서 안정화 대기 (켜고 바로는 오작동이 잦음)
  Serial.println("PIR 센서 준비 중 (30초)...");
  delay(30000);

  setLeds(true, false);
  bot.sendMessage(CHAT_ID, "✅ Rayder 시작! 보안 모드로 동작합니다.\n/help 로 명령어를 확인하세요.", "");
  lastStudyTick = millis();
}

// ===== loop =====
void loop() {
  unsigned long now = millis();

  // 1초마다 텔레그램 메시지 확인
  if (now - lastBotCheck > BOT_INTERVAL) {
    int count = bot.getUpdates(bot.last_message_received + 1);
    while (count) {
      handleMessages(count);
      count = bot.getUpdates(bot.last_message_received + 1);
    }
    lastBotCheck = millis();
  }

  now = millis();

  if (!systemOn) {
    lastStudyTick = now;  // 꺼져 있는 동안은 공부 시간에 안 들어가게
    return;
  }

  bool motion = (digitalRead(PIR_PIN) == HIGH);

  if (mode == SECURITY) runSecurity(motion, now);
  else                  runStudy(motion, now);

  lastStudyTick = now;
}
