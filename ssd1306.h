#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_RESET LED_BUILTIN //4
Adafruit_SSD1306 display(OLED_RESET);

#define XPOS 0
#define YPOS 1


#define LOGO16_GLCD_HEIGHT 16 
#define LOGO16_GLCD_WIDTH  16 


#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif
unsigned long ulSecs2000_timer=0;

void initDisplay()   {                

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  // init done
  Serial.println("Inizializzo........\n");
  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  //display.display();
  //delay(2000);

  // Clear the buffer.
  display.clearDisplay();
  Serial.println ("Pulisco Display ..... \n");
  // draw a single pixel
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,25);
  display.print("Start...");
  display.setTextSize(1);
  display.setCursor(0,5);
  display.print("DHT_Logger IR");
  display.display();
  delay(500);
}


