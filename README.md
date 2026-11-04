# Rayder IoT 보안 프로젝트

## 프로젝트 개요

ESP32 DevKitC를 이용해 침입 감지와 공부 시간 측정을 하는 IoT 프로젝트입니다. Telegram 봇으로 원격 제어와 알림을 받을 수 있습니다.

## 주요 기능

- **보안 모드**: PIR 센서가 움직임을 감지하면 부저와 빨간 LED로 알리고, Telegram 메시지를 보냅니다.
- **공부 도우미 모드**: 움직임이 감지되는 동안 착석 시간을 누적하고, 목표 시간을 채우면 부저와 Telegram으로 알려줍니다.
- **Telegram 명령어**: 상태 확인, 시스템 켜기/끄기, 경보 끄기, 모드 전환, 공부 목표 설정을 지원합니다.

## 부품 및 라이브러리

- 보드: ESP32 DevKitC 38핀 (USB-C)
- 센서: PIR 센서
- 출력: 능동 부저, 초록 LED, 빨간 LED
- Arduino 라이브러리: UniversalTelegramBot, ArduinoJson

## 핀 연결

| 부품 | ESP32 핀 |
| --- | --- |
| PIR 센서 OUT | GPIO 13 |
| 부저 + | GPIO 25 |
| 초록 LED | GPIO 26 |
| 빨간 LED | GPIO 27 |

<aside>
💡

LED에는 저항을 연결합니다.

</aside>

## Telegram 명령어

| 명령어 | 기능 |
| --- | --- |
| `/start`, `/help` | 명령어 안내 |
| `/status` | 현재 상태 확인 |
| `/on` | 시스템 켜기 |
| `/off` | 시스템 끄기 |
| `/alarm_off` | 경보 끄기 |
| `/security` | 보안 모드로 전환 |
| `/study` | 공부 도우미 모드로 전환 |
| `/goal 60` | 공부 목표를 60분으로 설정 |

## 설정할 값

코드의 Wi‑Fi 이름과 비밀번호, Telegram 봇 토큰, 채팅 ID를 본인 값으로 입력합니다.

```cpp
const char* WIFI_SSID = "와이파이이름";
const char* WIFI_PASS = "와이파이비밀번호";
#define BOT_TOKEN "텔레그램_봇_토큰"
#define CHAT_ID   "내_채팅_ID"
```

<aside>
⚠️

실제 값이 들어간 코드는 공개 저장소에 올리지 않도록 주의하세요.

</aside>

## 동작 참고

- ESP32가 켜지면 Wi‑Fi 연결 후 PIR 센서를 30초간 안정화합니다.
- 보안 모드 경보는 최대 1분간 울리고, 알림은 30초 쿨다운을 둡니다.
- 공부 도우미 모드에서는 1분 동안 움직임이 감지되지 않으면 자리를 비운 것으로 처리합니다.
