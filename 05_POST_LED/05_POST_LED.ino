#include "ArduinoJson.h"
#include <stdlib.h>
#include "WiFiEsp.h"
#include "SoftwareSerial.h"
SoftwareSerial Serial1(2, 3); // TX, RX (아두이노는 RX, TX 순)

char ssid[] = "HYOSUNG-2G"; // 공유기 WiFi 이름
char pass[] = "hshs7001"; // 공유기 비밀번호

WiFiEspServer server(80); // 80번 포트 사용하는 서버 객체를 생성

const int LED = 4;
unsigned int on_off = 0;

void getHeaderValue(char* src, char* dst, char* start) {
  char* pStart = strstr(src, start);
  int cnt = 0;
  if (pStart != NULL) {
    for (int i = strlen(start); pStart + i != strstr(pStart, "\r\n"); i++) {
      dst[cnt++] = pStart[i];
    }
    dst[cnt] = '\0';
  }
}

void setup() {
  pinMode(LED, OUTPUT);
  
  // debugging을 위한 시리얼 초기화
  Serial.begin(9600);

  // ESP-01 모듈을 위한 시리얼 초기화
  Serial1.begin(9600);

  // ESP 모듈 초기화
  WiFi.init(&Serial1);

  Serial.print("연결을 시도 중 입니다. WPA SSID: ");
  Serial.println(ssid);

  // WiFi 네트워크 연결
  WiFi.begin(ssid, pass);

  Serial.println("연결 되었습니다.");

  // 아두이노 웹 서버 시작
  server.begin();
}

void loop() {
  // 클라이언트 접속여부 확인 (Listen)
  WiFiEspClient client = server.available(); // (Listen)

  if (client) {
    Serial.println("새로운 클라이언트 접속");

    // Django 서버로부터 전달받은 요청처리
    char* buf = (char*)malloc(sizeof(char) * 500); // 요청받은 내용 저장공간 500Byte
    int index = 0;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        buf[index++] = c;
      }
      else {
        char* dataLength = (char*)malloc(sizeof(char) * 50); // 50Byte 메모리 할당
        getHeaderValue(buf, dataLength, "Content-Length: ");
        int length = atoi(dataLength); // 문자열을 정수로 변환

        // buf 내용 출력
        for (int i = 0; i < index; i++) {
          Serial.print(buf[i]);
        }

        // JSON 문자열을 요청받은 문자열로부터 끄집어 내기
        if (length > 0) {
          for (int i = 0; i < strlen(dataLength); i++) {
            dataLength[i] = '\0';
          }

          int cnt = 0;
          char* body = strstr(buf, "\r\n\r\n");
          int newLineLength = strlen("\r\n\r\n");
          for (int i = newLineLength; i < length + newLineLength; i++) {
            dataLength[cnt++] = body[i];
          }
          dataLength[cnt] = '\0';
          Serial.print("직렬화 된 JSON: ");
          Serial.println(dataLength);

          // 직렬화 된 JSON을 역직렬화하여 JSON 객체로 변경
          StaticJsonDocument<50> doc;
          deserializeJson(doc, dataLength);
          const char* type = doc["type"];
          const char* action = doc["action"];

          if (strcmp("LED", type) == 0) {
              if (strcmp("on", action) == 0) {
                // LED 켜기
                Serial.println("LED가 켜졌습니다.");
                // 1. LED 켜는 Arduino 코드를 작성
                on_off = 1;
              }
              else {
                // LED 끄기
                Serial.println("LED가 꺼졌습니다.");
                // 2. LED 끄는 Arduino 코드를 작성
                on_off = 0;
              }
              digitalWrite(LED, !on_off);
          }
        }
        free(dataLength);
        free(buf);

        // JSON 응답
        // 응답 헤더정보
        client.print("HTTP/1.1 200 OK\r\n");
        client.print("Content-Type: application/json;charset=utf-8\r\n");
        client.print("Server: Arduino\r\n");
        client.print("Access-Control-Allow-Origin: *\r\n");
        client.print("\r\n");
    
        // 응답 바디정보 (JSON 응답)
        String body = "";
        DynamicJsonDocument doc(50);
        doc["message"] = "success";
        doc["type"] = "LED";
        doc["action"] = on_off == 1 ? "on" : "off";
        serializeJson(doc, body);
        client.print(body);
        client.print("\r\n");
        Serial.print("응답 바디정보: ");
        Serial.println(body);
        
        client.flush(); // 클라이언트에게 보내줄 정보를 누락없이 마무리하여 보내주도록 하는 함수
        client.stop(); // 서버와 클라이언트 간의 연결을 끊어주는 역할
        Serial.println("클라이언트 연결 끊김");
      }
    }
  }
}
