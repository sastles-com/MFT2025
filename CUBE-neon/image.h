#pragma once

#include "common.h"
#include "vector-.h"
#include "quaternion.h"
#include <FastLED.h>

#define MAX_SELECT 5

namespace neon{
    class Image{
        private:
            TwoWire *wire;
            HWCDC *serial;

            int _width, _height;
//            CRGB allColor;

            int half_width, half_height;

            int mode_flg;
            int count;
            int random_hue;
            
        public:
            Image(){
                _width  = MASK_WIDTH;
                half_width = (int)MASK_WIDTH/2;
                _height = MASK_HEIGHT;
                half_height = (int)MASK_HEIGHT/2;
            };

//            void makeColorMap()

            void setMode(int _mode){
                mode_flg = _mode;
                random_hue = random(255);

            };

            CRGB uv2pixel(float u, float v){
                int uu = (int)(half_width*u) + half_width;
                int vv = (int)(half_height*v) + half_height;
                uint8_t _mask = (uint8_t)mask[vv][uu];
                int _s = 255;
                int _h = 255;
                int _v = min(255, (int)_mask);

                switch(mode_flg){
                    case 0:   // full-single-color mode
                        _h = random_hue;
                        _v = 0;
                        
                        break;
                    case 1:   // full rainbow
                        _h = random_hue;
                        break;
                    case 2:   // horizontal rainbow mode
                        _h = (count - vv);
                        if(_h < 0){
                            _h = 256 + _h;
                        }
                        break;
                    case 3:   // random
                       _h = count;
                        break;
                    case 4:   // full-single-color mode
                        _h = random(255);
                        break;
                };
                return CHSV(_h, _s, _v);
            };


//            imu::Vector<2> pos2uv(imu::Vector<3> vec){
//                float x, y;
//            }

            
            void init(TwoWire *w, HWCDC *s){
                wire = w;
                serial = s;

                mode_flg = 1;
                random_hue = random(255);
            };
            void update(){
                count++;
                if(count > 255)   count = 0;
            };
    };
}
