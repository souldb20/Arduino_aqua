//06

#include "NDelayFunc.h"
#include "ArduinoJson.h"
#include <stdlib.h>
#include "WiFiEsp.h"
#include "SoftwareSerial.h"

#define SensorPin 7
#define Offset 0.33  // ph7.0 측정 시 오프셋
#define LED 13
//#define samplingInterval 20
//#define printInterval 800
#define ArrayLength 40
int pHArray[ArrayLength];
int pHArrayIndex = 0;

static float density, voltage;

SoftwareSerial Serial1(2, 3); // TX, RX

char ssid[] = "HYOSUNG-2G"; // 공유기 WiFi 이름
char pass[] = "hshs7001"; // 공유기 비밀번호
char server[] = "192.168.0.62"; // 여러분의 장고 서버IP주소

WiFiEspClient client;

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

NDelayFunc nDelayReadPH(250, readPH);
NDelayFunc nDelayPrintPH(10000, printPH);
NDelayFunc nDelayHttpRequestPost(10000, httpRequestPost);


void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);
  WiFi.init(&Serial1);
  Serial.print("연결을 시도 중 입니다. WPA SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  Serial.println("연결 되었습니다.");
}

void loop() {
  nDelayReadPH.run();
  nDelayPrintPH.run();
  nDelayHttpRequestPost.run();
}
