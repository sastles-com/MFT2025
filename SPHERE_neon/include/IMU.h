#pragma once

//https://qiita.com/crawd4274/items/811155424ee588fc936a
#include "vector-.h"
#include "quaternion.h"



#include "common.h"

// IMU sensor conditional includes
#if defined(IMU_SENSOR_BMI270)
  #include <M5Unified.h>
  #include "imu/SphereIMUManager.h"
  // BMI270 will be accessed via M5Unified and SphereIMUManager
#elif defined(IMU_SENSOR_BNO055)
  #include <Adafruit_BNO055.h>
  #include <Adafruit_Sensor.h>
  #include <utility/imumaths.h>
#else
  // Default to BNO08x for backward compatibility
  #include <Adafruit_BNO08x.h>
  // For SPI mode, we need a CS pin
  #define BNO08X_CS 10
  #define BNO08X_INT 9
  // For SPI mode, we also need a RESET
  //#define BNO08X_RESET 5
  // but not for I2C or UART
  #define BNO08X_RESET -1
  Adafruit_BNO08x bno08x(BNO08X_RESET);
  sh2_SensorValue_t sensorValue;
#endif

//
#define SDA 2
#define SCL 1




namespace neon{
    class IMU{
        private:
#if defined(IMU_SENSOR_BMI270)
            SphereIMUManager sphereIMU;  // BMI270用IMUマネージャー
#else
            TwoWire *wire;
            HWCDC *serial;
#endif
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
#if defined(IMU_SENSOR_BMI270)
                Serial.println("Setting BMI270 reports via SphereIMUManager");
                // BMI270はSphereIMUManagerで管理されるため、ここでは設定不要
#else
                Serial.println("Setting desired reports");
                if (!bno08x.enableReport(SH2_GAME_ROTATION_VECTOR)) {
                    Serial.println("Could not enable game rotation vector");
                }
#endif
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
#if defined(IMU_SENSOR_BMI270)
                Serial.println("Initializing BMI270 via SphereIMUManager");
                if (sphereIMU.initialize()) {
                    Serial.println("BMI270 Found and initialized!");
                } else {
                    Serial.println("Failed to find BMI270 chip");
                    while (1) {
                        delay(10);
                    }
                }
                setReports();
                _offset = imu::Quaternion(0.0, 1.0, 0.0, 0.0);
#else
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
                _offset = imu::Quaternion(0.0, 1.0, 0.0, 0.0);
#endif
            };

            
            
            void update(){
#if defined(IMU_SENSOR_BMI270)
                sphereIMU.update();
                
                // SphereIMUManagerからクォータニオンを取得してimu::Quaternionに変換
                auto sphere_quat = sphereIMU.getOrientation();
                game_rotation = imu::Quaternion(sphere_quat.w, sphere_quat.x, sphere_quat.y, sphere_quat.z);
                
                if(count >= 100){
                #if _DEBUG
                    Serial.print("BMI270 Game Rotation Vector - r: ");
                    Serial.print(game_rotation.w());
                    Serial.print(" i: ");
                    Serial.print(game_rotation.x());
                    Serial.print(" j: ");
                    Serial.print(game_rotation.y());
                    Serial.print(" k: ");
                    Serial.println(game_rotation.z());
                #endif
                    count = 0;
                }
                count++;
#else
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
                        }
                        break;
                }
#endif
            }
        };
    };
