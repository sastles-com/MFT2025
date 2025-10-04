#include <DabbleESP32.h>


#include <FastLED.h>
#include <Wire.h>


#include "common.h"


#include "LED.h"
neon::LED led;
#include "IMU.h"
neon::IMU sensor;
#include "Image.h"
neon::Image image;


#define UPDATES_PER_SECOND 100


#define DEBUG true

#define SDA 13
#define SCL 15

int count = 0;
int _select = 0;
bool button_flg;



float roll, pitch, yaw;
bool arrow_up_flg = false;
bool arrow_down_flg = false;
bool arrow_left_flg = false;
bool arrow_right_flg = false;

imu::Quaternion xrot;
imu::Quaternion yrot;
imu::Quaternion zrot;


void setup() {
    
    Serial.begin(115200);
    while (!Serial)
        delay(10); // will pause Zero, Leonardo, etc until serial console opens
  
    Serial.println("neon");
    Wire.begin(SDA, SCL);
    Wire.setPins(SDA, SCL);


    led.init(&Wire, &Serial);
    sensor.init(&Wire, &Serial);
    image.init(&Wire, &Serial);

//    for(int my = 0; my < MASK_HEIGHT; my ++){
//        for(int mx = 0; mx < MASK_WIDTH; mx ++){
//            uint8_t _mask = (uint8_t)mask[my][mx];
////            int _mask = 0;
//            Serial.printf(" %d", _mask);
//        }
//        Serial.println(" ");
//    }



//    for(int i = 0; i < TOTAL_PIXELS; i ++){
//        Serial.printf("%d, %d, %d \n", layout[i][0], layout[i][1], layout[i][2]);
//        
//    }
    button_flg = false;
    Dabble.begin("cube_neon");

    roll = 0.0;
    pitch = 0.0;
    yaw = 0.0;

//    xrot = imu::Quaternion(0.0, 1.0, 0.0, 0.0);
//    yrot = imu::Quaternion(0.0, 0.0, 1.0, 0.0);
//    zrot = imu::Quaternion(0.0, 0.0, 0.0, 1.0);
}




void loop() {
    // put your main code here, to run repeatedly:
    Dabble.processInput();
    if (GamePad.isSelectPressed())
    {
        button_flg = true;
    }else{
        if(button_flg){
            _select ++;
            if(_select >= MAX_SELECT) _select = 0;
            image.setMode(_select);
            Serial.printf("Select %d \n", _select);
            button_flg = false;
        }
    }
    if (GamePad.isUpPressed()){
        arrow_up_flg = true;        
    }else{
        if(arrow_up_flg){
            pitch += 0.1;
            if(pitch > PI) pitch -= PI;
            Serial.printf("pitch %f \n", pitch);
            arrow_up_flg = false;
        }
        xrot = imu::Quaternion((float)(cos(pitch/2.0)), (float)(sin(pitch/2.0)), 0.0, 0.0);
//        xrot = sensor.fromAxisAngle(imu::Vector(1.0, 0.0, 0.0), pitch);
    }
    if (GamePad.isDownPressed()){
        arrow_down_flg = true;        
    }else{
        if(arrow_down_flg){
            roll += 0.1;
            if(roll > PI) roll -= PI;
            Serial.printf("roll %f \n", roll);
            arrow_down_flg = false;
        }
        yrot = imu::Quaternion((float)(cos(pitch/2.0)), 0.0, (float)(sin(pitch/2.0)), 0.0);
//        yrot = sensor.fromAxisAngle(imu::Vector(0.0, 1.0, 0.0), roll);
    }
    if (GamePad.isRightPressed()){
        arrow_right_flg = true;        
    }else{
        if(arrow_right_flg){
            yaw += 0.1;
            if(yaw > PI) yaw -= PI;
            Serial.printf("yaw %f \n", yaw);
            arrow_right_flg = false;
        }
        zrot = imu::Quaternion((float)(cos(pitch/2.0)), 0.0, 0.0, (float)(sin(pitch/2.0)));
//        zrot = sensor.fromAxisAngle(imu::Vector(0.0, 0.0, 1.0), yaw);
    }

    sensor.setOffset(xrot, yrot, zrot);


    sensor.update();
    image.update();
    for(int i = 0; i < TOTAL_LAYOUTS; i ++){
        float pos[3];
        for(int a = 0; a < 3; a ++){
            pos[a] = layout[i][a];
        }
        imu::Vector<3> vec = sensor.rotate(imu::Vector<3>(pos[0], pos[1], pos[2]));
//        imu::Vector<3> vec = sensor.rotate(imu::Vector<3>(-pos[2], pos[1], pos[0]));
//        imu::Vector<3> vec = imu::Vector<3>(-pos[2], pos[1], pos[0]);
//        imu::Vector<3> vec = imu::Vector<3>(pos[0], pos[1], pos[2]);

        float u = _atan2(_sqrt(vec[0]*vec[0] + vec[2]*vec[2]), vec[1]);
        float v = _atan2(vec[0], vec[2]);
        CRGB _color = image.uv2pixel(u, v);

        led.setPixel(i, _color); 
//        Serial.printf("(%f, %f, %f) \n", vec[0], vec[1], vec[2]);
        if(i == 0){
//            Serial.printf("(%f, %f, %f) \n", vec[0], vec[1], vec[2]);
//            Serial.printf("(%f, %f) \n", u, v);
//            Serial.printf("(%f, %f)  {%3d %3d %3d} \n", u, v, _color.red, _color.green, _color.blue);
        }
    }
    
//
//    for(int i = 0; i < LED_NUM; i ++){
//        if(count == i) {
//            led.setPixel(i, CRGB::White);          
//        }else{
//            led.setPixel(i, CRGB::Black);          
//        }
//    }



    led.update();
    count ++;
    if(count > LED_NUM){
        count = 0;
    }
//    Serial.print("test\n");
    delay(1);
}
