//07

#include "NDelayFunc.h"
#include "ArduinoJson.h"
#include <stdlib.h>
#include "WiFiEsp.h"
#include "SoftwareSerial.h"

#define samplingInterval 250
#define printInterval 10000
#define postInterval 60000

#define SensorPin 7
#define Offset 0.33  // ph7.0 측정 시 오프셋
#define LED 13
#define ArrayLength 40
int pHArray[ArrayLength];
int pHArrayIndex = 0;

static float density, voltage;

SoftwareSerial Serial1(2, 3); // TX, RX

char ssid[] = "HYOSUNG-2G"; // 공유기 WiFi 이름
char pass[] = "hshs7001"; // 공유기 비밀번호
char server[] = "192.168.0.62"; // 여러분의 장고 서버IP주소

WiFiEspClient client;

WiFiEspServer arduino_server(80); // 80번 포트 사용하는 아두이노 서버 객체를 생성

#define PumpPin = 4;
unsigned int on_off = 0;

// Header 정보를 끄집어내는 함수
// src: 응답받은 문자열
// dst: 특정 헤더의 Value를 저장할 배열
// start: 헤더명 + ": " 까지
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

double avergearray(int* arr, int number) {
  int i;
  int max, min;
  double avg;
  long amount = 0;
  if (number <= 0) {
    Serial.println("Error number for the array to avraging!/n");
    return 0;
  }
  if (number < 5) {
    for (i = 0; i < number; i++) {
      amount += arr[i];
    }
    avg = amount / number;
    return avg;
  } else {
    if (arr[0] < arr[1]) {
      min = arr[0];
      max = arr[1];
    } else {
      min = arr[1];
      max = arr[0];
    }
    for (i = 2; i < number; i++) {
      if (arr[i] < min) {
        amount += min;
        min = arr[i];
      } else {
        if (arr[i] > max) {
          amount += max;
          max = arr[i];
        } else {
          amount += arr[i];
        }
      }
    }
    avg = (double)amount / (number - 2);
  }
  return avg;
}

void printPH(){
  Serial.print("Voltage:");
  Serial.print(voltage, 2);
  Serial.print("    density value: ");
  Serial.println(density, 2);
  digitalWrite(LED, digitalRead(LED) ^ 1);
}

void readPH(){
    pHArray[pHArrayIndex++] = analogRead(SensorPin);
    if (pHArrayIndex == ArrayLength) pHArrayIndex = 0;
    voltage = avergearray(pHArray, ArrayLength) * 5.0 / 1024;
    density = 3.5 * voltage + Offset;
}

void httpRequestPost() {
   client.stop();

  if (client.connect(server, 8000)) {  
    /*
     * 2. POST 요청
     */
    String body = "";
    DynamicJsonDocument doc(128);
    doc["density"] = density;

    serializeJson(doc, body);
  
    // Request Header 정보와 직렬화된 JSON 객체를 서버로 전달
    client.print("POST /ph/ HTTP/1.1\r\n");
    client.print("Host: ");
    client.print(server);
    client.print(":8000\r\n");
    client.print("Content-Type: application/json\r\n");
    client.print("Content-Length: ");
    client.print(body.length());
    client.print("\r\n\r\n");
    client.print(body);
    client.print("\r\n");
  
    /*
     * 장고 서버로부터 전달받은 응답(Response)
     * 현재 ESP-01 모듈에서 POST 요청에 대한 응답을 받지 못해
     * 아래 코드는 현재로써는 정상동작하지 않습니다.
     * 차후 더 좋은 장비를 이용하시면 정상적으로 응답을 받을 수 있습니다.
     */
    // 응답 내용을 저장할 메모리 할당
    char* buf = (char*)malloc(sizeof(char) * 512); // 512Byte 크기 할당 (동적배열할당 / C언어 내용)
    int index = 0;
  
    while(client.connected()) {
      if (client.available()) { // 서버로부터 전달받은 응답 중 아두이노가 처리하지 않은 내용이 존재하는지 여부
        char c = client.read();
        buf[index++] = c;
      }
      else {
        client.stop();
      }
    }
  
    // 응답 내용 중 서버로부터 전달받은 데이터의 길이 값을 가져온다.
    char* dataLength = (char*)malloc(sizeof(char) * 50); // 50Byte 메모리 할당
    getHeaderValue(buf, dataLength, "Content-Length: ");
    int length = atoi(dataLength); // 문자열을 정수로 변환
  
    // JSON 문자열을 응답 문자열로부터 끄집어 내기
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
      Serial.print("JSON 문자열: ");
      Serial.println(dataLength);
  
      // JSON 문자열을 JSON 객체로 변경
      StaticJsonDocument<50> doc;
      deserializeJson(doc, dataLength);
      const char* message = doc["message"];
      Serial.println(message);
    }
    free(dataLength);
  
    // 응답 내용을 출력
    for (int i = 0; i < index; i++) {
      Serial.print(buf[i]);
    }
    free(buf); // 할당 받았던 메모리를 해제    
  }
}
void changePump(){
  if(density >=  6 && density <= 8){
      // PUMP 켜기
      Serial.println("수중 펌프 작동");
      // 1. PUMP 켜는 Arduino 코드를 작성
      on_off = 1;
    }
  else {
    // PUMP 끄기
    Serial.println("수중 펌프 중단");
    // 2. PUMP 끄는 Arduino 코드를 작성
    on_off = 0;
  }
  digitalWrite(PumpPin, on_off);
}

void postPump() {
  // 클라이언트 접속여부 확인 (Listen)
  WiFiEspClient client = arduino_server.available(); // (Listen)

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

          if (strcmp("PUMP", type) == 0) {
              if (strcmp("on", action) == 0) {
                // PUMP 켜기
                Serial.println("수중 펌프 작동");
                // 1. PUMP 켜는 Arduino 코드를 작성
                on_off = 1;
              }
              else {
                // PUMP 끄기
                Serial.println("수중 펌프 중단");
                // 2. PUMP 끄는 Arduino 코드를 작성
                on_off = 0;
              }
              digitalWrite(PumpPin, on_off);
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
        doc["type"] = "PUMP";
        doc["action_result"] = on_off == 1 ? "on" : "off";
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

NDelayFunc nDelayReadPH(samplingInterval, readPH);
NDelayFunc nDelayPrintPH(printInterval, printPH);
NDelayFunc nDelayHttpRequestPost(postInterval, httpRequestPost);
NDelayFunc nDelayChangePump(postInterval, changePump);

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);
  WiFi.init(&Serial1);
  Serial.print("연결을 시도 중 입니다. WPA SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  Serial.println("연결 되었습니다.");

  // 아두이노 웹 서버 시작
  arduino_server.begin();
}

void loop() {
  nDelayReadPH.run();
  nDelayPrintPH.run();
  nDelayHttpRequestPost.run();
  nDelayChangePump.run();
  postPump();
}
