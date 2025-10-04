#pragma once

//https://qiita.com/crawd4274/items/811155424ee588fc936a
#include "vector-.h"
#include "quaternion.h"



#include "common.h"

// Basic demo for readings from Adafruit BNO08x
#include <Adafruit_BNO08x.h>
//#include <Adafruit_Sensor.h>
//#include <utility/imumaths.h>


// For SPI mode, we need a CS pin
#define BNO08X_CS 10
#define BNO08X_INT 9

// For SPI mode, we also need a RESET
//#define BNO08X_RESET 5
// but not for I2C or UART
#define BNO08X_RESET -1

Adafruit_BNO08x bno08x(BNO08X_RESET);
sh2_SensorValue_t sensorValue;

//
//#define SDA 13
//#define SCL 15




namespace neon{
    class IMU{
        private:
            TwoWire *wire;
            HWCDC *serial;
//            USBCDC *serial;

            imu::Quaternion game_rotation;
            imu::Quaternion _offset;
            imu::Vector<3> gravity;
            int count;
        public:
            IMU(){
                count = 0;
            };            


            // Here is where you define the sensor outputs you want to receive
            void setReports(void) {
                Serial.println("Setting desired reports");
//                if (!bno08x.enableReport(SH2_ACCELEROMETER)) {
//                    Serial.println("Could not enable accelerometer");
//                }
//                if (!bno08x.enableReport(SH2_GYROSCOPE_CALIBRATED)) {
//                    Serial.println("Could not enable gyroscope");
//                }
//                if (!bno08x.enableReport(SH2_MAGNETIC_FIELD_CALIBRATED)) {
//                    Serial.println("Could not enable magnetic field calibrated");
//                }
//                if (!bno08x.enableReport(SH2_LINEAR_ACCELERATION)) {
//                    Serial.println("Could not enable linear acceleration");
//                }
//                if (!bno08x.enableReport(SH2_GRAVITY)) {
//                    Serial.println("Could not enable gravity vector");
//                }
//                if (!bno08x.enableReport(SH2_ROTATION_VECTOR)) {
//                    Serial.println("Could not enable rotation vector");
//                }
//                if (!bno08x.enableReport(SH2_GEOMAGNETIC_ROTATION_VECTOR)) {
//                    Serial.println("Could not enable geomagnetic rotation vector");
//                }
                if (!bno08x.enableReport(SH2_GAME_ROTATION_VECTOR)) {
                    Serial.println("Could not enable game rotation vector");
                }
//                if (!bno08x.enableReport(SH2_STEP_COUNTER)) {
//                   Serial.println("Could not enable step counter");
//                }
//                if (!bno08x.enableReport(SH2_STABILITY_CLASSIFIER)) {
//                   Serial.println("Could not enable stability classifier");
//                }
//                if (!bno08x.enableReport(SH2_RAW_ACCELEROMETER)) {
//                   Serial.println("Could not enable raw accelerometer");
//                }
//                if (!bno08x.enableReport(SH2_RAW_GYROSCOPE)) {
//                   Serial.println("Could not enable raw gyroscope");
//                }
//                if (!bno08x.enableReport(SH2_RAW_MAGNETOMETER)) {
//                    Serial.println("Could not enable raw magnetometer");
//                }
//                if (!bno08x.enableReport(SH2_SHAKE_DETECTOR)) {
//                    Serial.println("Could not enable shake detector");
//                }
//                if (!bno08x.enableReport(SH2_PERSONAL_ACTIVITY_CLASSIFIER)) {
//                    Serial.println("Could not enable personal activity classifier");
//                }
            }


            imu::Vector<3> rotate(imu::Vector<3> input){
                imu::Quaternion q = game_rotation * _offset;
                imu::Vector<3> v = q.rotate(input);

                return v;
            }

            void addQuaternion(imu::Quaternion q){
                _offset = _offset * q;
              
            }

            void setOffset(imu::Quaternion q1, imu::Quaternion q2, imu::Quaternion q3){
                _offset = q1*q2*q3;
              
            }



            void init(TwoWire *w, HWCDC *s){
                wire = w;
                serial = s;

                // Try to initialize!
                if (!bno08x.begin_I2C()) {
                  // if (!bno08x.begin_UART(&Serial1)) {  // Requires a device with > 300 byte
                  // UART buffer! if (!bno08x.begin_SPI(BNO08X_CS, BNO08X_INT)) {
                  serial->println("Failed to find BNO08x chip");
                  while (1) {
                    delay(10);
                  }
                }
                serial->println("BNO08x Found!");
                for (int n = 0; n < bno08x.prodIds.numEntries; n++) {
                    serial->print("Part ");
                    serial->print(bno08x.prodIds.entry[n].swPartNumber);
                    serial->print(": Version :");
                    serial->print(bno08x.prodIds.entry[n].swVersionMajor);
                    serial->print(".");
                    serial->print(bno08x.prodIds.entry[n].swVersionMinor);
                    serial->print(".");
                    serial->print(bno08x.prodIds.entry[n].swVersionPatch);
                    serial->print(" Build ");
                    serial->println(bno08x.prodIds.entry[n].swBuildNumber);
                }
                setReports();
                /*
//                serial->println("---");
                if (!bno08x.enableReport(SH2_GRAVITY)) {
                    Serial.println("Could not enable gravity vector");
                }

//                Serial.println("---");
                for(int i = 0; i < 100; i ++){
                    if (bno08x.wasReset()) {
                        Serial.print("sensor was reset ");
    //                  setReports();
                    }             
                    if (!bno08x.getSensorEvent(&sensorValue)) {
//                        return;
                    }
    
                    switch (sensorValue.sensorId) {
                        case SH2_GRAVITY:
                            gravity = imu::Vector<3>(sensorValue.un.gravity.x, sensorValue.un.gravity.y, sensorValue.un.gravity.z);
                            serial->print("Gravity - x: ");
                            serial->print(sensorValue.un.gravity.x);
                            serial->print(" y: ");
                            serial->print(sensorValue.un.gravity.y);
                            serial->print(" z: ");
                            serial->println(sensorValue.un.gravity.z);
                            
                            break;
                    };
                }
                count = 0;

                bno08x = Adafruit_BNO08x(BNO08X_RESET);
                if (!bno08x.begin_I2C()) {
                    serial->println("Failed to find BNO08x chip");
                    while (1) {
                        delay(10);
                    }
                }
                setReports();
                */

                _offset = imu::Quaternion(0.0, 1.0, 0.0, 0.0);
            };

            
            
            void update(){
                if (bno08x.wasReset()) {
                    serial->print("sensor was reset ");
//                  setReports();
                }             
                if (!bno08x.getSensorEvent(&sensorValue)) {
                    return;
                }

                switch (sensorValue.sensorId) {
                    case SH2_GRAVITY:
                        #if _DEBUG
                        if(count >= 30){
                            serial->print("Gravity - x: ");
                            serial->print(sensorValue.un.gravity.x);
                            serial->print(" y: ");
                            serial->print(sensorValue.un.gravity.y);
                            serial->print(" z: ");
                            serial->println(sensorValue.un.gravity.z);
                        }
                        #endif
                        break;                  
                    case SH2_GAME_ROTATION_VECTOR:
                        game_rotation = imu::Quaternion(sensorValue.un.gameRotationVector.real, sensorValue.un.gameRotationVector.i, sensorValue.un.gameRotationVector.j, sensorValue.un.gameRotationVector.k);

                        if(count >= 100){
                        #if _DEBUG
                            serial->print("Game Rotation Vector - r: ");
                            serial->print(game_rotation.w());
                            serial->print(" i: ");
                            serial->print(game_rotation.x());
                            serial->print(" j: ");
                            serial->print(game_rotation.y());
                            serial->print(" k: ");
                            serial->println(game_rotation.z());
                        #endif
                            count = 0;
                        }
                        count ++;
                        break;
                    case SH2_LINEAR_ACCELERATION:
                        if(count >= 100){
                        #if _DEBUG
                            Serial.print("Linear Acceration - x: ");
                            Serial.print(sensorValue.un.linearAcceleration.x);
                            Serial.print(" y: ");
                            Serial.print(sensorValue.un.linearAcceleration.y);
                            Serial.print(" z: ");
                            Serial.println(sensorValue.un.linearAcceleration.z);
                        #endif
                            count = 0;
                        break;
                };
 
            };
        
        };
    };
}
