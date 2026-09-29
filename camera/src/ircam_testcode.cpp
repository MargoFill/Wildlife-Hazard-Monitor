#include <Arduino.h>
#define FLSH_PIN 4
#include <WebServer.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <esp_wifi.h>
#include <soc/rtc_cntl_reg.h>
#include <soc/soc.h>

// wifi
//#define WIFI_SSID "" //-> Name of Hotspot
//#define WIFI_PASS "" //-> Password

// camera pins Ai Thinker
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// camera init
bool cameraInit(framesize_t frame_size, pixformat_t pixel_format, int jpeg_quality) {
    esp_wifi_set_ps(WIFI_PS_NONE);              // no power save
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // disable brownout

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = pixel_format;
    config.frame_size = frame_size;
    config.jpeg_quality = jpeg_quality;
    config.fb_count = 1;

    return esp_camera_init(&config) == ESP_OK;
}

WebServer server(80);

void setup() {
    Serial.begin(115200);
    Serial.println();

    // camera
    if (!cameraInit(FRAMESIZE_VGA, PIXFORMAT_JPEG, 12)) {
        Serial.println("Camera error");
        for (;;);
    }

    // wifi -> Hotspot
    WiFi.mode(WIFI_AP);
    
    // Making up a password for the network
    WiFi.softAP("ESP32_Camera_WIFI", "12345678"); 
    
    Serial.println();
    Serial.println("Wi-Fi hotspot created");
    Serial.print("IP address for browser: ");
    Serial.println(WiFi.softAPIP()); // port 192.168.4.1


    //wifi -> Mobile data
    /*
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("Connected: ");
    Serial.println(WiFi.localIP());*/

    // server
    server.on("/", []() {
        camera_fb_t* fb = esp_camera_fb_get();
        if (fb) {
            server.setContentLength(fb->len);
            server.send(200, "image/jpeg", "");
            server.sendContent((const char*)fb->buf, fb->len);
        }
        esp_camera_fb_return(fb);
    });
    server.begin();
}

void loop() {
    server.handleClient();
}