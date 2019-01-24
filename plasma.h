void drawPlasma();
inline uint8_t fastCosineCalc( uint16_t );
int XY(int , int );
void drawPixel(int , int , CRGB );

//Plasma variables
unsigned long nextFrame = 0;
int wait = 10;
long frameCount = 256000;

 CRGBArray < NUM_LEDS / 2 > plasmaLeds; // half the number of actual leds

const uint8_t  cos_wave[256]  =
{ 0, 0, 0, 0, 1, 1, 1, 2, 2, 3, 4, 5, 6, 6, 8, 9, 10, 11, 12, 14, 15, 17, 18, 20, 22, 23, 25, 27, 29, 31, 33, 35, 38, 40, 42,
  45, 47, 49, 52, 54, 57, 60, 62, 65, 68, 71, 73, 76, 79, 82, 85, 88, 91, 94, 97, 100, 103, 106, 109, 113, 116, 119,
  122, 125, 128, 131, 135, 138, 141, 144, 147, 150, 153, 156, 159, 162, 165, 168, 171, 174, 177, 180, 183, 186,
  189, 191, 194, 197, 199, 202, 204, 207, 209, 212, 214, 216, 218, 221, 223, 225, 227, 229, 231, 232, 234, 236,
  238, 239, 241, 242, 243, 245, 246, 247, 248, 249, 250, 251, 252, 252, 253, 253, 254, 254, 255, 255, 255, 255,
  186, 183, 180, 177, 174, 171, 168, 165, 162, 159, 156, 153, 150, 147, 144, 141, 138, 135, 131, 128, 125, 122,
  119, 116, 113, 109, 106, 103, 100, 97, 94, 91, 88, 85, 82, 79, 76, 73, 71, 68, 65, 62, 60, 57, 54, 52, 49, 47, 45,
  42, 40, 38, 35, 33, 31, 29, 27, 25, 23, 22, 20, 18, 17, 15, 14, 12, 11, 10, 9, 8, 6, 6, 5, 4, 3, 2, 2, 1, 1, 1, 0, 0, 0, 0
};

const uint8_t  exp_gamma[256]  =
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3,
  4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 12, 12, 12, 13, 13, 14, 14, 14, 15, 15,
  16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 23, 23, 24, 24, 25, 26, 26, 27, 28, 28, 29, 30, 30, 31, 32, 32, 33,
  34, 35, 35, 36, 37, 38, 39, 39, 40, 41, 42, 43, 44, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
  61, 62, 63, 64, 65, 66, 67, 68, 70, 71, 72, 73, 74, 75, 77, 78, 79, 80, 82, 83, 84, 85, 87, 89, 91, 92, 93, 95, 96, 98,
  99, 100, 101, 102, 105, 106, 108, 109, 111, 112, 114, 115, 117, 118, 120, 121, 123, 125, 126, 128, 130, 131, 133,
  135, 136, 138, 140, 142, 143, 145, 147, 149, 151, 152, 154, 156, 158, 160, 162, 164, 165, 167, 169, 171, 173, 175,
  177, 179, 181, 183, 185, 187, 190, 192, 194, 196, 198, 200, 202, 204, 207, 209, 211, 213, 216, 218, 220, 222, 225,
  227, 229, 232, 234, 236, 239, 241, 244, 246, 249, 251, 253, 254, 255
};

void drawPlasma() {
  frameCount++ ;
  //  patternTimer = 55 - ( patternSpeed * 5);
  uint16_t t = fastCosineCalc((92 * frameCount) / 100); //time displacement - fiddle with these til it looks good...
  uint16_t t2 = fastCosineCalc((55 * frameCount) / 100);
  uint16_t t3 = fastCosineCalc((83 * frameCount) / 100);
  for (uint8_t y = 0; y < NUM_LEDS_PER_STRIP / 2; y++)
  {
    for (uint8_t x = 0; x < NUM_STRIPS; x++)
    {
      uint8_t r = fastCosineCalc(((x << 3) + (t >> 1) + fastCosineCalc((t2 + (y << 3)))));  //Calculate 3 seperate plasma waves, one for each color channel
      uint8_t g = fastCosineCalc(((y << 3) + t + fastCosineCalc(((t3 >> 2) + (x << 3)))));
      uint8_t b = fastCosineCalc(((y << 3) + t2 + fastCosineCalc((t + x + (g >> 2)))));
      //uncomment the following to enable gamma correction
      r = pgm_read_byte_near(exp_gamma + r);
      g = pgm_read_byte_near(exp_gamma + g);
      b = pgm_read_byte_near(exp_gamma + b);
      drawPixel(x, y, CRGB(r, g, b)); // Is this the fastest method to update the draw buffer with colors?
    }
  }
  //  copy the data to from the plasmaLeds to leds
//  for (int i = 0; i < NUM_STRIPS; i++) {

//    leds(i*NUM_LEDS_PER_STRIP, (i*NUM_LEDS_PER_STRIP)+(NUM_LEDS_PER_STRIP / 2) - 1) = plasmaLeds(i*NUM_LEDS_PER_STRIP, ((i*NUM_LEDS_PER_STRIP)+NUM_LEDS_PER_STRIP / 2) - 1);
//    leds((i+1)*NUM_LEDS_PER_STRIP - 1,((i+1)*NUM_LEDS_PER_STRIP)- (NUM_LEDS_PER_STRIP / 2)) = plasmaLeds(i*NUM_LEDS_PER_STRIP, ((i*NUM_LEDS_PER_STRIP)+NUM_LEDS_PER_STRIP / 2) - 1);

    //strip 0
    leds(0, NUM_LEDS_PER_STRIP / 2 - 1) = plasmaLeds(0, NUM_LEDS_PER_STRIP / 2 - 1);
    leds(NUM_LEDS_PER_STRIP - 1, NUM_LEDS_PER_STRIP / 2) = plasmaLeds(0, NUM_LEDS_PER_STRIP / 2 - 1);
    
    // strip 1
    leds(NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP*3 / 2 - 1) = plasmaLeds(NUM_LEDS_PER_STRIP / 2, NUM_LEDS_PER_STRIP  - 1);
    leds(NUM_LEDS_PER_STRIP*2 - 1, NUM_LEDS_PER_STRIP*3 / 2) = plasmaLeds(NUM_LEDS_PER_STRIP / 2, NUM_LEDS_PER_STRIP  - 1);   

    //strip 2
    leds(NUM_LEDS_PER_STRIP*2, NUM_LEDS_PER_STRIP*5 / 2 - 1) = plasmaLeds(NUM_LEDS_PER_STRIP , NUM_LEDS_PER_STRIP*3 / 2 - 1);
    leds(NUM_LEDS_PER_STRIP*3 - 1, NUM_LEDS_PER_STRIP*5 / 2) = plasmaLeds(NUM_LEDS_PER_STRIP , NUM_LEDS_PER_STRIP*3 / 2 - 1); 


    //strip 3
    leds(NUM_LEDS_PER_STRIP*3, NUM_LEDS_PER_STRIP*7 / 2 - 1) = plasmaLeds(NUM_LEDS_PER_STRIP*3 / 2, NUM_LEDS_PER_STRIP*2  - 1);
    leds(NUM_LEDS_PER_STRIP*4 - 1, NUM_LEDS_PER_STRIP*7 / 2) = plasmaLeds(NUM_LEDS_PER_STRIP*3 / 2, NUM_LEDS_PER_STRIP*2  - 1); 
//
//    leds(NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP / 2 - 1) = plasmaLeds(0, NUM_LEDS_PER_STRIP / 2 - 1);
//    leds(NUM_LEDS_PER_STRIP - 1, NUM_LEDS_PER_STRIP / 2) = plasmaLeds(0, NUM_LEDS_PER_STRIP / 2 - 1);
//  }
}

inline uint8_t fastCosineCalc( uint16_t preWrapVal)
{
  uint8_t wrapVal = (preWrapVal % 255);
  if (wrapVal < 0) wrapVal = 255 + wrapVal;
  return (pgm_read_byte_near(cos_wave + wrapVal));
}

int XY(int x, int y) {
  if (y > NUM_LEDS_PER_STRIP / 2) {
    y = NUM_LEDS_PER_STRIP / 2;
  }
  if (y < 0) {
    y = 0;
  }
  if (x > NUM_STRIPS) {
    x = NUM_STRIPS;
  }
  if (x < 0) {
    x = 0;
  }

  if (x % 2 == 1) { // column is odd, therefore reversed
    return (x * (NUM_LEDS_PER_STRIP / 2 + 1) + (NUM_LEDS_PER_STRIP / 2 - y));
  } else {
    return (x * (NUM_LEDS_PER_STRIP / 2 + 1) + y);
  }
}

void drawPixel(int x, int y, CRGB c) {
  if ((x >= 0) & (x <= NUM_STRIPS) & (y >= 0) & (y <= NUM_LEDS_PER_STRIP / 2)) {
    plasmaLeds[XY(x, y)] = c;
  }
}
