////////////////////////////////////////////////////////////////////////////
// pcg850vs_gadget_1_Rev_B_v1_2_SBH
//
// Based on https://github.com/blackjetrock/pcg850vs-sd-gadget
// 
// Modified: SBH 04/08/22   v1.0   Added comments
//           SBH 05/08/22   v1.1   Removed surplus code, added transmit status graph
//           SBH 05/08/22   v1.2   Blinking cursor
//           AJM 21/08/22   v2.0   Merged SBH code, altered version and put back in menu options
//           AJM 22/08/22   v2.1   Tidied version string and menu
//           AJM 27/08/22   v2.2   Added SSIO reception interrupt. Temp comment out due to menu scrolling
//                                 not working.
//           AJM 28/08/22   v2.3   Added 'Send File' menu option. This sends the file data directly from an
//                                 SD card file so has no buffer involved and hence no length limit.
//                                 Re-arranged menus so they always fit on a screen.
////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ARDUINO IDE SETTINGS
// Board:          Generic STM32F1 Series
// Board Part No.: BlackPill F103C8
// Upload method:  STM32CubeProgrammer (SWD)
// Preferences -> Additional Boards Manager URL: https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json
//
///////////////////////////////////////////////////////////////////////////

#define VERSION_STRING "V2.3"

#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define ACK1  1
#define ACK2  0       // Works with PCG850

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Define variables
//
////////////////////////////////////////////////////////////////////////////////////////////////////
Sd2Card card;
SdVolume volume;
SdFile root;

int filename_index = 0;

const int chipSelect = 4;

// Pins for Sharp 11 pin interface data lines
const int IFD0Pin  = PB8;
const int IFD1Pin  = PB9;
const int IFD2Pin  = PB4;
const int IFD3Pin  = PB3;
const int IFD4Pin  = PB12;
const int IFD5Pin  = PB13;
const int IFD6Pin  = PB14;
const int IFD7Pin  = PB15;

// SIO pins
const int SIORTSPin = IFD0Pin;
const int SIODTRPin = IFD1Pin;
const int SIORXDPin = IFD2Pin;
const int SIOTXDPin = IFD3Pin;
const int SIOCDPin  = IFD4Pin;
const int SIOCTSPin = IFD5Pin;
const int SIODSRPin = IFD6Pin;
const int SIOCIPin  = IFD7Pin;

// SSIO pins
const int SIOBUSYPin = IFD0Pin;
const int SIODOUTPin = IFD1Pin;
const int SIOXINPin  = IFD2Pin;
const int SIOXOUTPin = IFD3Pin;
const int SIODINPin  = IFD4Pin;
const int SIOACKPin  = IFD5Pin;
const int SIOEX1Pin  = IFD6Pin;
const int SIOEX2Pin  = IFD7Pin;

// Debug outputs
const int statPin   = PC13;
const int dataPin   = PC14;


#define NIF 8

const int if_pin[NIF] = {
  IFD0Pin,
  IFD1Pin,
  IFD2Pin,
  IFD3Pin,
  IFD4Pin,
  IFD5Pin,
  IFD6Pin,
  IFD7Pin,
};
  
File myFile;

// Capture file
File dumpfile;


#define DEBUG_SERIAL 0
#define DIRECT_WRITE 0

// STM32F103C8 has only 20k of SRAM
//const int MAX_BYTES = 10000;
const int MAX_BYTES = 10000; 
const int button1Pin = PA0;
const int button2Pin = PA1;
const int button3Pin = PC15;

typedef unsigned char BYTE;

typedef void (*FPTR)();
typedef void (*CMD_FPTR)(String cmd);

#define NUM_BUTTONS 3

// Debounce
#define MAX_BUT_COUNT 6

int but_pins[NUM_BUTTONS] = {button1Pin, button2Pin, button3Pin};

typedef struct _BUTTON
{
  int     count;
  boolean pressed;
  boolean last_pressed;   // For edge detection
  FPTR    event_fn;
} BUTTON;

BUTTON buttons[NUM_BUTTONS];

enum ELEMENT_TYPE
  {
    BUTTON_ELEMENT = 10,
    SUB_MENU,
    MENU_END,
  };

struct MENU_ELEMENT
{
  enum ELEMENT_TYPE type;
  char *text;
  void *submenu;
  void (*function)(struct MENU_ELEMENT *e);
};


struct MENU_ELEMENT *current_menu;
struct MENU_ELEMENT *last_menu;
struct MENU_ELEMENT *the_home_menu;
unsigned int menu_selection = 0;
unsigned int menu_size = 0;

#define MAX_LISTFILES 7
#define MAX_NAME 20

const char *filetag = "@file ";

MENU_ELEMENT listfiles[MAX_LISTFILES+1];
int num_listfiles;
char names[MAX_LISTFILES][MAX_NAME];
char selected_file[MAX_NAME+1];
char current_file[MAX_NAME+1];

// Where the received data goes
char stored_bytes[MAX_BYTES] = "Test data example. 01234567890";

int stored_bytes_index = 0;

// How many bytes in the buffer. We have a default for testing without a Microtan
int bytecount = 24;

// communication with ISRs

volatile int data_byte;
volatile int bit_count;
volatile int got_byte = 0;
volatile int state = 0;
volatile int data_bit = 2;

// We build the byte packet in this int, it can be longer than
// 8 bits as it has th estart bit, stop bit and parity if we have it on

volatile unsigned int capture_bits_len = 0;
volatile unsigned int capture_bits = 0xffff;

// Flag that indicates we are looking for a start bit
volatile int waiting_for_start = 1;

#define START_BIT 2

// Flag that indicates we have a bit or bits. This indicates activity on the serial
// line that may need to be attended to
volatile int got_bit = 0;

// How many bits we have received
volatile int number_of_bits = 0;

// Bit value of the N bits
volatile int bit_value = 0;

// Bit period in us
// default to 9600
//volatile int bit_period = 833;
volatile int bit_period = 102;


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Define prototypes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void but_ev_null();
void but_ev_up();
void but_ev_down();
void but_ev_select();
void button_ssio_mode();


void send_serial_byte(int byte)
{
  int j;
  
  // Start bit
  digitalWrite(SIORXDPin, HIGH);
  delayMicroseconds(bit_period);
  
  // Now 8 data bits, LS first
  for(j=0; j<8; j++)
    {
      if( byte & 1 )
	{
	  digitalWrite(SIORXDPin, LOW);
	  delayMicroseconds(bit_period);
	}
      else
	{
	  digitalWrite(SIORXDPin, HIGH);
	  delayMicroseconds(bit_period);
	}
      
      byte >>= 1;
    }
  
  // Stop bit
  digitalWrite(SIORXDPin, LOW);
  delayMicroseconds(bit_period);
  
  // Put a little character delay in
  delayMicroseconds(10*bit_period);
  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Sends a file 
//
////////////////////////////////////////////////////////////////////////////////////////////////////

// This sends a file from SD card to the 850. The buffer is not used, so any size file should
// be handled fine, assuming the 850 can receive it

void send_file(boolean oled_nserial)
{
  int i, j;
  int bytecount = 0;
  String fullBarGraph = "************************";
  int byte_sent;
  
  if( oled_nserial )
    {
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("Sending file ");
      display.println(current_file);
      display.display();
      delay(1000);
    }
  else
    {
      Serial.print("Sending file '");
      Serial.print(current_file);
      Serial.println("'");
    }
  
  myFile = SD.open(current_file);
  
  if (myFile)
    {
      // Read through to find size of file
      while (myFile.available())
	{
	  myFile.read();
	  bytecount++;
	}

      // close the file:
      myFile.close();

      // and re-open
      myFile = SD.open(current_file);

      i = 0;
      
      // Read from the file and send it
      while (myFile.available())
	{
	  byte_sent = myFile.read();	  

	  send_serial_byte(byte_sent);
	  i++;
	  
	  // Bar graph status update
	  if( oled_nserial )
	    {
	      display.clearDisplay();
	      display.setCursor(0,0);
	      display.println(fullBarGraph.substring(0, int(20*i/bytecount)));
	      display.setCursor(0,20);
	      display.print(i);
	      display.print(" of ");
	      display.print(bytecount);
	      display.print(" bytes");
	      display.display();
	    }
	  else
	    {
	      Serial.print(i);
	    }
	}
      
      // close the file:
      myFile.close();

      // Add end of file marker if it wasn't the last character sent
      if( byte_sent != 0x1a )
	{
	  send_serial_byte(0x1a);
	}
      
      if( oled_nserial )
	{
	  display.clearDisplay();
	  display.setCursor(0,0);
	  display.println("Done.");
	  display.display();
	  delay(1000);
	}
      else
	{
	  Serial.println("Done");
	}
    }
  else
    {
      // if the file didn't open, print an error:
      if( oled_nserial )
	{
	  display.println("Error opening");
	  display.println(current_file);
	  display.display();
	  delay(1000);
	}
      else
	{
	  Serial.print("Error opening ");
	  Serial.println(current_file);
	}
    }
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Sends the currently loaded data bytes to the PC-G850 using the currently
// set bit period (baud rate)
//
// Signals are all inverted, so start bit is HIGH (logical 0)
// Data transmitted LS first, 8N1 is format
// We add a 0x1a to the end as an end of file marker
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void send_databytes(boolean oled_nserial)
{
  int i, j;
  int byte;
  String fullBarGraph = "************************";

  // Add end of file marker if there isn't one
  if( stored_bytes[bytecount-1] != 0x1a )
    {
      stored_bytes[bytecount++] = 0x1a;
    }

  if( oled_nserial )
    {
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("Sending ");
      display.println(bytecount);
      display.println(" bytes");
      display.display();
      delay(1000);
    }
  else
    {
      Serial.print("Sending ");
      Serial.print(bytecount);
      Serial.println(" bytes");
    }

  // Send byte
  for(i=0; i<bytecount; i++)
    {
      // Send one byte
      byte = stored_bytes[i];

      // Start bit
      digitalWrite(SIORXDPin, HIGH);
      delayMicroseconds(bit_period);

      // Now 8 data bits, LS first
      for(j=0;j<8;j++)
	{
	  if( byte & 1 )
	    {
	      digitalWrite(SIORXDPin, LOW);
	      delayMicroseconds(bit_period);
	    }
	  else
	    {
	      digitalWrite(SIORXDPin, HIGH);
	      delayMicroseconds(bit_period);
	    }

	  byte >>= 1;
	}

      // Stop bit
      digitalWrite(SIORXDPin, LOW);
      delayMicroseconds(bit_period);

      // Put a little character delay in
      delayMicroseconds(10*bit_period);

      // Bar graph status update
      if( oled_nserial )
          {
            display.clearDisplay();
            display.setCursor(0,0);
            display.println(fullBarGraph.substring(0, int(20*i/bytecount)));
            display.setCursor(0,20);
            display.print(i);
            display.print(" of ");
            display.print(bytecount);
            display.print(" bytes");
            display.display();
          }
        else
          {
            Serial.print(i);
          }
    }
//

    if( oled_nserial )
    {
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("Done.");
      display.display();
      delay(1000);
    }
  else
    {
      Serial.println("Done");
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Set 11 pin interface pins to inputs. This is the safest state we can be in
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void set_if_inputs()
{
  int i;
  for(i=0; i<NIF; i++)
    {
      pinMode(if_pin[i], INPUT);
    }
  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 
//
////////////////////////////////////////////////////////////////////////////////////////////////////


// Set interface to SIO mode

void set_if_sio()
{
  set_if_inputs();
}

// Set interface to SSIO mode

void set_if_ssio()
{
  set_if_inputs();

  // Set up control lines to allow 850 to send and receive
  // SSIO data
  
  pinMode(SIOBUSYPin, INPUT);
  pinMode(SIODOUTPin, INPUT);
  pinMode(SIOXOUTPin, INPUT);
  // Drive signals to allow sending by 850
  digitalWrite(SIOACKPin, LOW);    // Wait

  pinMode(SIOACKPin,  OUTPUT);

  // Drive signals to allow sending by 850
  digitalWrite(SIOACKPin, LOW);

  detachInterrupt(digitalPinToInterrupt(SIOTXDPin));
  attachInterrupt(digitalPinToInterrupt(SIOXOUTPin), ssio_isr,  RISING);
  
}


// Read the state of the interface
int read_if_state()
{

  int i;
  int val = 0;
  
  for(i=NIF-1; i>=0; i--)
    {
      val *=2;
      val += digitalRead(if_pin[i]);
    }
  return(val);
}

//
// Allow interaction with serial monitor
//

// Commands
int indx = 0;


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void cmd_next(String cmd)
{
  indx++;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 
//
////////////////////////////////////////////////////////////////////////////////////////////////////


void cmd_prev(String cmd)
{
  indx--;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void cmd_index(String cmd)
{
  String arg;
  
  Serial.println("INDEX");
  arg = cmd.substring(1);
  Serial.println(arg);
  
  indx = arg.toInt();
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Modify the buffer
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void cmd_modify(String cmd)
{
  String arg;
  
  Serial.println("MOD");
  arg = cmd.substring(1);

  if( indx <= MAX_BYTES )
    {
      stored_bytes[indx] = arg.toInt();
    }
}

#define DISPLAY_COLS 16


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Displays the file data, with offset and address (from start address stored in file data)
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void cmd_display(String cmd)
{
  int i;
  int address = stored_bytes[10]*256+stored_bytes[11]-12;
  
  char ascii[DISPLAY_COLS+2];
  char c[2];
  char line[50];
  
  int ascii_i = 0;
  ascii[0] ='\0';
  
  Serial.print("Byte Count:");
  Serial.print(bytecount);
  Serial.print("  Index:");
  Serial.println(indx);

  for(i=0; i<bytecount; i++)
    {
      if( (i%DISPLAY_COLS)==0 )
	{
	  sprintf(line, "%s\n%04X %04X:", ascii, i, address); 	  
	  Serial.print(line);
	  ascii_i = 0;
	}

      sprintf(line, "%02X ", stored_bytes[i]);
      Serial.print(line);
      
      if( isprint(stored_bytes[i]) )
	{
	  sprintf(c, "%c", stored_bytes[i]);
	}
      else
	{
	  c[0] ='.';
	}
      ascii[ascii_i++] = c[0];
      ascii[ascii_i] = '\0';
      
      address++;
    }

  ascii[ascii_i++] = '\0';

  // Pad to the ascii position
  for(i=ascii_i-1; i<DISPLAY_COLS; i++)
    {
      Serial.print("   ");
    }
  
  Serial.print(ascii);
  Serial.print(" ");
  Serial.println("");
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Clear the buffer
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void cmd_clear(String cmd)
{
  bytecount = -1;    // We reset to -1 so we drop the leading spurious character

#if DIRECT_WRITE
  SD.remove(filename);
  dumpfile = SD.open("dumpfile.bin", FILE_WRITE);
#endif
  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Close the capture file
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void cmd_close(String cmd)
{
  dumpfile.close();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void cmd_send(String cmd)
{
  send_databytes(false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Deletes a file
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void cmd_deletefile(String cmd)
{
  String arg;
  
  arg = cmd.substring(strlen("delete "));

  Serial.print("Deleting file '");
  Serial.print(arg);
  Serial.println("'");
  
  SD.remove(arg);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Read the file with the given name into the buffer
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void core_read(String arg, boolean oled_nserial)
{
  if( oled_nserial )
    {
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("Reading file ");
      display.println(arg.c_str());
      display.display();
      delay(1000);
    }
  else
    {
      Serial.print("Reading file '");
      Serial.print(arg);
      Serial.println("'");
    }
  
  myFile = SD.open(arg);

  if (myFile)
    {
      // Read from the file and store it in the buffer
      bytecount = 0;
      
      while (myFile.available())
	{
	  stored_bytes[bytecount++] = myFile.read();
	  if( bytecount >= MAX_BYTES )
	    {
	      bytecount = MAX_BYTES;
	    }
	  
	}
      
      // close the file:
      myFile.close();

      if ( oled_nserial )
	{
	  display.print(bytecount);
	  display.println(" bytes read");
	  display.display();
	  delay(1000);
	}
      else
	{
	  Serial.print(bytecount);
	  Serial.println(" bytes read.");
	}
    }
  else
    {
      // if the file didn't open, print an error:
      if( oled_nserial )
	{
	  display.println("Error opening");
	  display.println(arg.c_str());
	  display.display();
	  delay(1000);
	}
      else
	{
	  Serial.print("Error opening ");
	  Serial.println(arg);
	}
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void cmd_readfile(String cmd)
{
  String arg;
  
  arg = cmd.substring(strlen("read "));

  core_read(arg, false);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// List the files on the SD card
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void cmd_listfiles(String cmd)
{
  File dir;
  
  dir = SD.open("/");

  // return to the first file in the directory
  dir.rewindDirectory();
  
  while (true) {
    
    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }

    Serial.print(entry.name());

    if (entry.isDirectory())
      {
	Serial.println("/");
      }
    else
      {
	// files have sizes, directories do not
	Serial.print("\t\t");
	Serial.println(entry.size(), DEC);
      }
    entry.close();
  }

  dir.close();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Initlaise the SD card
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void cmd_initsd(String cmd)
{
  if (!SD.begin(chipSelect)) {
    Serial.println("SD Card initialisation failed!");
  }
  else
    {
      Serial.println("SD card initialised.");
    }

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests if the the specified tag matches the bytes at the specified position of
// a 0x1A-terminated segment at the specified position of the buffer.
// Please note that len is the buffer size (from position zero) and not the segment.
//
////////////////////////////////////////////////////////////////////////////////////////////////////

static inline bool matches(const char *s, size_t len, int pos, const char *tag,
        size_t taglen)
{
    int i;
    for (i = 0; i < len && s[pos + i] != 0x1A && s[pos + i] == tag[i]; i++);
    return i == taglen;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Returns the index of the first occurence of the tag in the specified buffer
// or -1 if the tag has not been found.
//
////////////////////////////////////////////////////////////////////////////////////////////////////

static int index_of(const char *s, size_t len, const char *tag, size_t taglen)
{
    int i;
    for (i = 0; i < len && s[i] != 0x1A; i++)
      {
        if (matches(s, len, i, tag, taglen))
            return i;
      }
    return -1;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Retrieves the file name from the respective annotation tag stored in the 0x1A
// terminated segment of the specified buffer.
// Returns true if the file name is successfully retrieved, false otherwise.
//
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool get_filename(const char *s, size_t len, char *dst, size_t dstlen)
{
    int i, j;
    size_t taglen = strlen(filetag);
    if ((i = index_of(s, len, filetag, taglen)) == -1)
        return false;
    i += strlen(filetag);
    for (j = 0; j < dstlen - 1 && i < len && !strchr(" \x1A\t\n\r", s[i]);)
        dst[j++] = s[i++];
    if (j < dstlen)
        dst[j] = 0;
    return j > 1;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Writes the buffer to a file.
// Deletes any file that exists with the same name so that the resulting
// file is the same size as the buffer
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void core_writefile(boolean oled_nserial)
{
  char filename[20] = "U___.txt";
  int i;

  if (!get_filename(stored_bytes, MAX_BYTES, filename, 20))
    {
      do
        {
          sprintf(filename, "PCG%04d.DAT", filename_index++);
        }
      while( SD.exists(filename) );
    }
  
    if( oled_nserial )
    {
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("");
      display.print("Writing ");
      display.print(" ");
      display.println();
      display.print("'");
      display.print(filename);
      display.print("'");
      display.display();
    }
  else
    {
      Serial.println("");
      Serial.print("Writing ");
      Serial.print(bytecount);
      Serial.print(" bytes to '");
      Serial.print(filename);
      Serial.print("'");
      Serial.println("");
    }
  
  // Delete so we have no extra data if the file is currently larger than the buffer
  SD.remove(filename);
  
  // Open file for writing
  myFile = SD.open(filename, FILE_WRITE);

  if( myFile )
    {
      // Write data
      for(i=0; i<bytecount; i++)
	{
	  myFile.write(stored_bytes[i]);
	}
      
      myFile.close();

      if( oled_nserial )
	{
	  display.setCursor(0,3*8);
	  display.print(bytecount);
	  display.println(" bytes written");
	  display.display();
	}
      else
	{
	  Serial.print(bytecount);
	  Serial.println(" bytes written");
	}
    }
  else
    {
      if(oled_nserial)
	{
	  display.println("Could not open file");
	  display.display();

	}
      else
	{
	  Serial.println("Could not open file");
	}
    }

  if( oled_nserial )
    {
      delay(1000);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void cmd_writefile(String cmd)
{
  core_writefile(false);
}

const int NUM_CMDS = 14;

String cmd;
struct
{
  String cmdname;
  CMD_FPTR   handler;
} cmdlist [NUM_CMDS] =
  {
    {"m",           cmd_modify},
    {"clear",       cmd_clear},
    {"display",     cmd_display},
    {"next",        cmd_next},
    {"prev",        cmd_prev},
    {"i",           cmd_index},
    {"close",       cmd_close},
    {"write",       cmd_writefile},
    {"send",        cmd_send},
    {"list",        cmd_listfiles},
    {"initsd",      cmd_initsd},
    {"help",        cmd_help},
    {"read",        cmd_readfile},
    {"delete",      cmd_deletefile},
  };


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void cmd_help(String cmd)
{
  int i;
  
  for(i=0; i<NUM_CMDS; i++)
    {
      Serial.println(cmdlist[i].cmdname);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void run_monitor()
{
  char c;
  int i;
  String test;
  
  if( Serial.available() )
    {
      c = Serial.read();

      switch(c)
	{
	case '\r':
	case '\n':
	  // We have a command, process it
	  for(i=0; i<NUM_CMDS; i++)
	    {
	      test = cmd.substring(0, (cmdlist[i].cmdname).length());
	      if( test == cmdlist[i].cmdname )
		{
		  (*(cmdlist[i].handler))(cmd);
		}
	    }

	  cmd = "";
	  break;

	default:
	  cmd += c;
	  break;
	}
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// The switch menu/OLED display system
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void to_back_menu(struct MENU_ELEMENT *e)
{
  menu_selection = 0;
  current_menu = last_menu;
  draw_menu(current_menu, true);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void to_home_menu(struct MENU_ELEMENT *e)
{
  menu_selection = 0;
  current_menu = the_home_menu;
  draw_menu(current_menu, true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Sets PIO to all inputs 
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void button_all_inputs(MENU_ELEMENT *e)
{
  set_if_inputs();

  Serial.print("Interface now all inputs");

  display.setTextSize(1);      
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Interface all inputs");
  display.display();
  delay(1000);

  draw_menu(current_menu, true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Read and display the i/f as a byte
// Does not set all inputs, which allows SIO mode to be examined
// Or any other mode in the future
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void button_read_if(MENU_ELEMENT *e)
{
  Serial.print("button read if");
  while(1)
    {
      delay(1000);
      Serial.println(read_if_state());
      display.setTextSize(3);      
      display.clearDisplay();
      display.setCursor(0,0);
      display.println(read_if_state());
      display.display();
    }

  // Reset display parameters
  display.setTextSize(1);      
  display.clearDisplay();
  display.setCursor(0,0);
  display.display();
  
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Read and display interface data lines as a scope
// Does not set up as inputs, that is done in PIO Inputs menu option
// This allows SIO mode to be scoped
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void button_scope_if(MENU_ELEMENT *e)
{
  int s;
  int m;
  int b;
  
  Serial.print("Scoping interface");

  display.clearDisplay();
  
  for(int i=0; i<128; i++)
    {
      s = read_if_state();

      m = 1;
      
      for(b=0; b<8; b++)
	{
	  if( s & m )
	    {
	      display.drawPixel(i, b*8+4, 1);
	    }
	  else
	    {
	      display.drawPixel(i, b*8+0, 1);
	    }
	  m  <<= 1;
	}

      display.display();
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Clear the buffer
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void button_clear(MENU_ELEMENT *e)
{
  bytecount = -1;

  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Buffer Cleared");
  display.display();
  
  delay(1000);
  draw_menu(current_menu, true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Set new file name
//
////////////////////////////////////////////////////////////////////////////////////////////////////

// This sets the file name to use for writing data to the SD card
// used when flow control is in use as file name has to be set up before data sent.

void button_new_file(MENU_ELEMENT *e)
{

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Write the selected file to the buffer
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void button_write(MENU_ELEMENT *e)
{
  core_writefile(true);

  delay(1000);
  draw_menu(current_menu, true);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// The button function puts up to the first 7 files on screen then set sup a button handler
// which will display subsequent pages.
// We use the menu structures to display the names and allow selection
// File selected
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void button_select_file(MENU_ELEMENT *e)
{
  strcpy(selected_file, e->text);

  // Back a menu
  to_back_menu(e);
  
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Allow a file to be selected. The file name will be stored for a later 'read' command.
//
////////////////////////////////////////////////////////////////////////////////////////////////////

int file_offset = 0;

#define FILE_PAGE 7

void but_ev_file_up()
{
#if DEBUG
  Serial.print("Before-menu_selection: ");
  Serial.print(menu_selection);
  Serial.print("file_offset: ");
  Serial.println(file_offset);
#endif
  
  if( menu_selection == 0 )
    {
      if( file_offset == 0 )
	{
	  // Don't move
	}
      else
	{
	  // Move files back one
	  file_offset--;
	}
    }
  else
    {
      // Move cursor up
      menu_selection--;
    }

#if DEBUG
  Serial.print("Before-menu_selection: ");
  Serial.print(menu_selection);
  Serial.print("file_offset: ");
  Serial.println(file_offset);
#endif
  
  button_list(NULL);

  if( menu_selection >= menu_size )
    {
      menu_selection = menu_size - 1;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void but_ev_file_down()
{
#if DEBUG  
  Serial.print("Before-menu_selection: ");
  Serial.print(menu_selection);
  Serial.print("file_offset: ");
  Serial.println(file_offset);
#endif
  
  // Move cursor down one entry
  menu_selection++;
  
  // Are we off the end of the menu?
  if( menu_selection == menu_size )
    {
      // 
      if( menu_selection >= MAX_LISTFILES,1 )
	{
	  menu_selection--;

	  // If the screen is full then we haven't reached the end of the file list
	  // so move the list up one
	  if( menu_size == MAX_LISTFILES )
	    {
	      file_offset++;
	    }
	}
    }

  // We need to make sure cursor is on menu
  if( menu_selection >= menu_size )
    {
      menu_selection = menu_size - 1;
    }

#if DEBUG  
  Serial.print("Before-menu_selection: ");
  Serial.print(menu_selection);
  Serial.print("file_offset: ");
  Serial.println(file_offset);
  Serial.print("menu_size: ");
  Serial.println(menu_size);
#endif
  
  button_list(NULL);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Store file name and exit menu
// File can be read later 
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void but_ev_file_select()
{
  strcpy(current_file, listfiles[menu_selection].text);
  file_offset = 0;

  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Selected file");
  display.print(current_file);
  display.display();
  delay(1000);

  menu_selection = 0;
  to_home_menu(NULL);
  
  buttons[0].event_fn = but_ev_up;
  buttons[1].event_fn = but_ev_down;
  buttons[2].event_fn = but_ev_select;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void button_to_home(MENU_ELEMENT *e)
{
  to_home_menu(NULL);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void button_list(MENU_ELEMENT *e)
{
  File dir;
  int file_n = 0;
  num_listfiles = 0;
  int i;

  Serial.println("button_list");
  dir = SD.open("/");

  // return to the first file in the directory
  dir.rewindDirectory();
  
  while (num_listfiles < MAX_LISTFILES) {

    File entry =  dir.openNextFile();

    if (! entry) {
      // no more files
      // terminate menu
      listfiles[num_listfiles].text = "";
      listfiles[num_listfiles].type = MENU_END;
      listfiles[num_listfiles].submenu = NULL;
      listfiles[num_listfiles].function = button_select_file;
      entry.close();
      break;
    }

    
    // We don't allow directories and don't ount them
    if (entry.isDirectory())
      {
      }
    else
      {
#if DEBUG	
	Serial.print("BList-file_n:");
	Serial.print(file_n);
	Serial.print(entry.name());
	Serial.print("  num_listfiles:");
	Serial.println(num_listfiles);
#endif
	// Create a new menu element
	// we also don't want to display anything before the offset
	if( file_n >= file_offset )
	  {
	    strncpy(&(names[num_listfiles][0]), entry.name(), MAX_NAME);
	    //	display.println(&(names[nu);
	    listfiles[num_listfiles].text = &(names[num_listfiles][0]);
	    listfiles[num_listfiles].type = BUTTON_ELEMENT;
	    listfiles[num_listfiles].submenu = NULL;
	    listfiles[num_listfiles].function = button_select_file;
	    
	    num_listfiles++;
	  }
	// Next file
	file_n++;

      }
    entry.close();
    
  }

  dir.close();

  // terminate menu
  listfiles[num_listfiles].text = "";
  listfiles[num_listfiles].type = MENU_END;
  listfiles[num_listfiles].submenu = NULL;
  listfiles[num_listfiles].function = button_select_file;

  // We know how big the menu is now
  if( num_listfiles != 0 )
    {
      menu_size = num_listfiles;
    }

  // Button actions modified
  buttons[0].event_fn = but_ev_file_up;
  buttons[1].event_fn = but_ev_file_down;
  buttons[2].event_fn = but_ev_file_select;

  // Set up menu of file names
  current_menu = &(listfiles[0]);
  draw_menu(current_menu, false);

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Display the buffer
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#define COLUMNS 5
#define PAGE_LENGTH 30

int display_offset = 0;

void but_page_up()
{
  if( display_offset > PAGE_LENGTH )
    {
      display_offset -= PAGE_LENGTH;
    }
  else
    {
      display_offset = 0;
    }
  button_display(NULL);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void but_page_down()
{
  display_offset += PAGE_LENGTH;
  
  if( display_offset >= bytecount )
    {
      display_offset = bytecount-PAGE_LENGTH;
    }

  if( display_offset < 0 )
    {
      display_offset = 0;
    }
  
  button_display(NULL);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void but_page_exit()
{
  display_offset = 0;
  draw_menu(current_menu, true);

  buttons[0].event_fn = but_ev_up;
  buttons[1].event_fn = but_ev_down;
  buttons[2].event_fn = but_ev_select;

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void button_display(MENU_ELEMENT *e)
{
  int i;
  char ascii[17];
  char c[5];
  
  int ascii_i = 0;

  display.clearDisplay();
  
  for(i=0; (i<bytecount) && (i<PAGE_LENGTH); i++)
    {
      if( isprint(stored_bytes[i]) )
	{
	  sprintf(ascii, "%c", stored_bytes[i+display_offset]);
	}
      else
	{
	  sprintf(ascii, ".");
	}
      
      sprintf(c,     "%02X",  stored_bytes[i+display_offset]);

      display.setCursor(6*15+(i%COLUMNS)*1*6, 8*(i/COLUMNS+1));
      display.println(ascii);
      display.setCursor(10*0+(i%COLUMNS)*2*8, 8*(i/COLUMNS+1));
      display.print(c);

    }

  // Drop into page up and down and exit buttoin handlers
  buttons[0].event_fn = but_page_up;
  buttons[1].event_fn = but_page_down;
  buttons[2].event_fn = but_page_exit;

  display.display();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void button_send(MENU_ELEMENT *e)
{
  send_databytes(true);
  draw_menu(current_menu, true);
}

void button_send_file(MENU_ELEMENT *e)
{
  send_file(true);
  draw_menu(current_menu, true);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// read the current file from SD card
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void button_read(MENU_ELEMENT *e)
{
  core_read(current_file, true);

  draw_menu(current_menu, true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SIO menu
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void button_set_sio(MENU_ELEMENT *e)
{
  // Set the data directions for the SIO mode
  set_if_sio();
 
  // Set up control lines to allow 850 to send and receive
  // SIO data
  
  pinMode(SIOCTSPin, OUTPUT);
  
  pinMode(SIORXDPin, OUTPUT);
  pinMode(SIOCDPin,  OUTPUT);
  pinMode(SIODSRPin, OUTPUT);
  pinMode(SIOCIPin, OUTPUT);

  // Drive signals to allow sending by 850
  digitalWrite(SIOCTSPin, HIGH);  // HIGH to allow 850 to send
  digitalWrite(SIODSRPin, LOW);
  digitalWrite(SIOCDPin, LOW);
  digitalWrite(SIOCIPin, LOW);
  
  // Put RXD into inactive line state
  digitalWrite(SIORXDPin, LOW);
  
  // reset start bit wait
  waiting_for_start = 1;

  //reset data counter
  bytecount = 0;

  capture_bits = 0xffff;

  display.clearDisplay();
  display.setCursor(0,0);
  display.println("SIO Mode set");
  display.display();
  delay(1000);
  to_back_menu(NULL);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SSIO menu
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void button_set_ssio(MENU_ELEMENT *e)
{
  // Set the data directions for the SIO mode
  set_if_ssio();
 

  //reset data counter
  bytecount = 0;

  capture_bits = 0xffff;

  display.clearDisplay();
  display.setCursor(0,0);
  display.println("SSIO Mode set");
  display.display();
  
  delay(1000);
  to_back_menu(NULL);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// All PIO as inputs our side
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void button_set_inputs(MENU_ELEMENT *e)
{
  set_if_inputs();
}

struct MENU_ELEMENT pio_menu[] =
  {
    {BUTTON_ELEMENT, "PIO All Inputs",             NULL,     button_all_inputs},
    {BUTTON_ELEMENT, "Read PIO States",            NULL,     button_read_if},
    {BUTTON_ELEMENT, "Scope PIO",                  NULL,     button_scope_if},
    {BUTTON_ELEMENT, "Exit",                       NULL,     button_to_home},
    {MENU_END,       "",                           NULL,     NULL},
  };

struct MENU_ELEMENT sio_menu[] =
  {
    {BUTTON_ELEMENT, "Set SIO Mode",               NULL,     button_set_sio},
    {BUTTON_ELEMENT, "Set All Inputs",             NULL,     button_set_inputs},
    {BUTTON_ELEMENT, "Exit",                       NULL,     button_to_home},
    {MENU_END,       "",                           NULL,     NULL},
  };

struct MENU_ELEMENT ssio_menu[] =
  {
    {BUTTON_ELEMENT, "Set SSIO Mode",              NULL,     button_set_ssio},
    {BUTTON_ELEMENT, "Set All Inputs",             NULL,     button_set_inputs},
    {BUTTON_ELEMENT, "Capture data",               NULL,     button_ssio_mode},
    {BUTTON_ELEMENT, "Exit",                       NULL,     button_to_home},
    {MENU_END,       "",                           NULL,     NULL},
  };

struct MENU_ELEMENT mode_menu[] =
  {
   {SUB_MENU,       "SSIO setup",                 ssio_menu, NULL},
   {SUB_MENU,       "PIO setup",                  pio_menu,  NULL},
   {BUTTON_ELEMENT, "Exit",                       NULL,     button_to_home},
   {MENU_END,       "",                           NULL,     NULL},
  };

struct MENU_ELEMENT buffer_menu[] =
  {
   {BUTTON_ELEMENT, "Read file to buffer",        NULL,      button_read},
   {BUTTON_ELEMENT, "Display buffer",             NULL,      button_display},
   {BUTTON_ELEMENT, "Clear buffer",               NULL,      button_clear},
   {BUTTON_ELEMENT, "Send buffer to 850VS",       NULL,      button_send},
   {BUTTON_ELEMENT, "Write buffer to file",       NULL,      button_write},
   {BUTTON_ELEMENT, "Exit",                       NULL,     button_to_home},
   {MENU_END,       "",                           NULL,     NULL},
  };

struct MENU_ELEMENT home_menu[] =
  {
    {SUB_MENU,       "SIO setup",                  sio_menu,    NULL},
    {BUTTON_ELEMENT, "List SD",                    NULL,        button_list},
    {SUB_MENU,       "Buffer",                     buffer_menu, NULL},
    {BUTTON_ELEMENT, "Send file to 850VS",         NULL,        button_send_file},
    {SUB_MENU,       "Modes",                      mode_menu,   NULL},
    {MENU_END,       "",                           NULL,        NULL},
  };


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Clear flag indicates whether we redraw the menu text and clear the screen. Or not.
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void draw_menu(struct MENU_ELEMENT *e, boolean clear)
{
  int i = 0;
  char *curs = " ";
  char etext[20];

  Serial.println("draw_menu");
  // Clear screen
  if(clear,1)
    {
      display.clearDisplay();
    }
  
  while( e->type != MENU_END )
    {
      sprintf(etext, " %-19s", e->text);
      
      switch(e->type)
	{
	case BUTTON_ELEMENT:
	  display.setCursor(0, i*8);
	  //display.printChar(curs);
	  if( clear,1 )
	    {
	      display.println(etext);
	    }
	  break;

	case SUB_MENU:
	  display.setCursor(0, i*8);
	  //display.printChar(curs);
	  if ( clear,1 )
	    {
	      display.println(etext);
	    }
	  break;
	}
      e++;
      i++;
    }
  
  menu_size = i;

#if DEBUG
  Serial.print("menu_size:");
  Serial.println(menu_size);
#endif
  
  // Blank the other entries
  //make sure menu_selection isn't outside the menu
  if( menu_selection >= menu_size )
    {
      menu_selection = menu_size-1;
    }

  for(; i<MAX_LISTFILES; i++)
    {
      display.setCursor(0, i*8);
      display.println("               ");
    }

  for(i=0;i<menu_size;i++)
    {
      if( i == menu_selection )
	{
	  curs = ">";	  
	}
      else
	{
	  curs = " ";
	}

      display.setCursor(0, i*8);
      display.print(curs);
    }
  display.display();
  Serial.println("Draw_menu exit");
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Null button event function
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void but_ev_null()
{ 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void but_ev_up()
{
  if( menu_selection == 0 )
    {
      menu_selection = menu_size - 1;
    }
  else
    {
      menu_selection = (menu_selection - 1) % menu_size;
    }
  
  draw_menu(current_menu, false);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void but_ev_down()
{

  menu_selection = (menu_selection + 1) % menu_size;

  draw_menu(current_menu, false);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void but_ev_select()
{
  struct MENU_ELEMENT *e;
  int i = 0;
  
  // Do what the current selection says to do
  for(e=current_menu, i=0; (e->type != MENU_END); i++, e++)
    {
      if( i == menu_selection )
	{
	  switch(e->type)
	    {
	    case SUB_MENU:
	      current_menu = (MENU_ELEMENT *)e->submenu;
	      draw_menu(current_menu, true);
	      break;
	      
	    default:
	      // Do action
	      (e->function)(e);
	      break;
	    }
	}
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Initialise the buttons
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void init_buttons()
{
  Serial.println("Init buttons");
  for(int i=0; i<NUM_BUTTONS; i++)
    {
      buttons[i].count = 0;
      buttons[i].pressed = false;
      buttons[i].last_pressed = false;
      buttons[i].event_fn = but_ev_null;
    }

  buttons[0].event_fn = but_ev_up;
  buttons[1].event_fn = but_ev_down;
  buttons[2].event_fn = but_ev_select;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Button event routine
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void update_buttons()
{
  for(int i=0; i<NUM_BUTTONS; i++)
    {
      if( digitalRead(but_pins[i]) == LOW )
	{
	  if( buttons[i].count < MAX_BUT_COUNT )
	    {
	      buttons[i].count++;
	      if( buttons[i].count == MAX_BUT_COUNT )
		{
		  // Just got to MAX_COUNT
		  buttons[i].pressed = true;
		  Serial.println("Press");
		}
	    }
	}
      else
	{
	  if( buttons[i].count > 0 )
	    {
	      buttons[i].count--;
	      
	      if( buttons[i].count == 0 )
		{
		  // Just got to zero
		  buttons[i].pressed = false;
		}
	    }
	}
      
      // If button has gone from not pressed to pressed then we treat that as a key event
      if( (buttons[i].last_pressed == false) && (buttons[i].pressed == true) )
	{
	  Serial.println("Call ev");
	  (buttons[i].event_fn)();
	}

      buttons[i].last_pressed = buttons[i].pressed;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SSIO Mode
//
// We sit in a loop looking at the interface, displaying characters that have come in
//
////////////////////////////////////////////////////////////////////////////////////////////////////

volatile unsigned int ssio_data = 0;
volatile unsigned int ssio_data_len = 0;

void ssio_isr()
{

#if ACK1
  digitalWrite(SIOACKPin, HIGH);  
#endif
  
  // This is run when the clock line goes high
  // Sample data line and add data bit
  
  if( digitalRead(SIODOUTPin)==HIGH )
    {
      ssio_data <<= 1;
      ssio_data += 1;
    }
  else
    {
      ssio_data <<= 1;
    }

  ssio_data_len++;

  if( ssio_data_len == 8 )
    {
      stored_bytes[bytecount++] = ssio_data;
      if ( bytecount >= MAX_BYTES )
	{
	  bytecount = MAX_BYTES -1;
	}
      
      ssio_data_len = 0;
    }
#if ACK1
  digitalWrite(SIOACKPin, LOW);  
#endif
  
}

void button_ssio_mode(struct MENU_ELEMENT *e)
{
  int done = 0;
  char chstr[2] = " ";
  int data;
  
  // Remove the interrupt handler that receives SIO data
  detachInterrupt(digitalPinToInterrupt(SIOTXDPin));
  attachInterrupt(digitalPinToInterrupt(SIOXOUTPin), ssio_isr,  CHANGE);

#if 0
  display.clearDisplay();
  display.setCursor(0,0);
  display.display();
#endif

#if ACK1
  // Enable transmission
  digitalWrite(SIOACKPin, HIGH);
#endif
  
#if 0
  // Sit in loop looking for data to come in
  while(!done)
    {
    }
  
  // End of mode, re-install interrupt handler and exit
  attachInterrupt(digitalPinToInterrupt(SIOTXDPin), lowISR,  CHANGE);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Initialise the board
//
////////////////////////////////////////////////////////////////////////////////////////////////////

//HardwareSerial Serial1(PA10, PA9);
HardwareSerial Serial2(PB11, PB10);

void setup() {

  // Wait a while for the OLED display to start up
  delay(100);
  Wire.begin();

  // Set the interface to be all inputs, this is a safe state to leave things in
  // until we decide to do some work over the interface
  set_if_inputs();

  pinMode(PA0, INPUT);
  pinMode(PA1, INPUT);
  pinMode(PA2, INPUT);

  // Set SD card CS GPIO to be an output
  digitalWrite(PA4, LOW);
  pinMode(PA4, OUTPUT);
  
  digitalWrite(PA4, LOW);
  pinMode(PA4, OUTPUT);
  
  pinMode(statPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  //  digitalWrite(dataPin, LOW);
  
  // put your setup code here, to run once:
  Serial.begin(2000000);
  Serial2.begin(9600);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3c)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  Serial.println("");
  Serial.println("");
  Serial.println("PC-G850VS Gadget");
  Serial.println("");
  Serial.println("OLED OK.");
  
  // Statup screen
  display.clearDisplay();Serial.print("\nInitializing SD card...");
  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);             // Start at top-left corner
  display.println(F("PC-G850VS Gadget"));
  display.println(F(VERSION_STRING));
  display.display();
  
  Serial.print("\nInitializing SD card...");

#if 0  
  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  if (!card.init(SPI_HALF_SPEED, chipSelect)) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("* is a card inserted?");
    Serial.println("* is your wiring correct?");
    Serial.println("* did you change the chipSelect pin to match your shield or module?");

    display.println("SD Fail");
    display.display();
  } else {
    Serial.println("Wiring is correct and a card is present.");
    display.println("SD OK");
    display.display();
    delay(1000);

  }

  // print the type of card
  Serial.println();
  Serial.print("Card type:         ");
  switch (card.type()) {
  case SD_CARD_TYPE_SD1:
    Serial.println("SD1");
    break;
  case SD_CARD_TYPE_SD2:      while( digitalread(SIOBUSYPin) == LOW)
	{
	}

      while( digitalread(SIOBUSYPin) == HIGH)
	{
	}

    Serial.println("SD2");
    break;
  case SD_CARD_TYPE_SDHC:
    Serial.println("SDHC");
    break;
  default:
    Serial.println("Unknown");
  }
  // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
  if (!volume.init(card)) {
    Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
    while (1);
  }

  Serial.print("Clusters:          ");
  Serial.println(volume.clusterCount());
  Serial.print("Blocks x Cluster:  ");
  Serial.println(volume.blocksPerCluster());

  Serial.print("Total Blocks:      ");
  Serial.println(volume.blocksPerCluster() * volume.clusterCount());
  Serial.println();

  // print the type and size of the first FAT-type volume
  uint32_t volumesize;
  Serial.print("Volume type is:    FAT");
  Serial.println(volume.fatType(), DEC);

  volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
  volumesize *= volume.clusterCount();       // we'll have a lot of clusters
  volumesize /= 2;                           // SD card blocks are always 512 bytes (2 blocks are 1KB)
  Serial.print("Volume size (Kb):  ");
  Serial.println(volumesize);
  Serial.print("Volume size (Mb):  ");
  volumesize /= 1024;
  Serial.println(volumesize);
  Serial.print("Volume size (Gb):  ");
  Serial.println((float)volumesize / 1024.0);
#else
    
  if (!SD.begin(chipSelect)) {
    Serial.println("SD Card initialisation failed!");
    display.println("SD Fail");
    display.display();
    delay(1000);
  }
  else
    {
      Serial.println("SD card initialised.");
      display.println("SD OK");
      display.display();
      delay(1000);
    }

#endif
  
  // Byte count
  bytecount = strlen(&(stored_bytes[0]));
  
  // Menu state
  current_menu = &(home_menu[0]);
  last_menu = &(home_menu[0]);
  the_home_menu = last_menu;
  to_home_menu(NULL);

  // Initialise buttons
  init_buttons();

  // We want an interrupt on rising and falling edges of the TXD signal
  attachInterrupt(digitalPinToInterrupt(SIOTXDPin), lowISR,  CHANGE);
  
  //  attachInterrupt(digitalPinToInterrupt(SIOTXDPin), highISR, RISING);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Flow control
//
////////////////////////////////////////////////////////////////////////////////////////////////////

// Flow control

void flow_control_task()
{
  #if 0
  // See if the buffer is nearly full, if so, drive the handshake line to stop more data arriving
  // and write the buffer contents to SD card. The 'New File' menu option is used to set up a new
  // file on the SD card for data.

  if( bytecount > (MAX_BYTES * 7 / 10) )
    {
      // Buffer nearly full, hold off more data and unload this to SD card
      digitalWrite(SIOCTSPin, LOW);  // LOW stops data coming from 850

      // Delay for a while to allow in-progress traffic to finish
      delay(1000);

      // Status message
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("");
      display.print("SD write ");
      display.print(" ");
      display.println();
      display.print("'");
      display.print(filename);
      display.print("'");
      display.display();

      // Write the buffer contents to SD card

      // Clear the buffer
      // We use 0 here as we don't care about the extra character that appear at the start
      bytecount = 0;
      
      digitalWrite(SIOCTSPin, HIGH);    // Allow data to flow again
    }
  #endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Main loop
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {
  int i;
  char c;

  // Update the button state
  update_buttons();

  // Run the serial monitor
  run_monitor();

  if( Serial2.available(),0 )
    {
      c = Serial2.read();
      Serial.print(" S");
      Serial.println(c,HEX); 
    }

  // Run a task that handles flow control on the data coming in to us
  flow_control_task();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// The ISRs look at edges on the TXD line. The delays between the edges are
// processed to get the data stream.
// 
// This is called when a falling edge is detected on the TXD pin
// Despite the interrupt funtion name, this is now called on both rising and falling edges
//
////////////////////////////////////////////////////////////////////////////////////////////////////

volatile long t, t2 = 0;
long delta = 0;
volatile long hz2400=0, hz1200=0;
int in_byte = 0;

void lowISR()
{
  // Debug
  // This line indicates when a n interrupt is running. 
  digitalWrite(statPin, HIGH);

  // If the TXD line is now high then it was a period of zeros, but as the TXD line is inverted
  // that means it was a period of ones
  
  if ( digitalRead(SIOTXDPin) == HIGH )
    {
      digitalWrite(dataPin, HIGH);
      digitalWrite(dataPin, LOW);
      digitalWrite(dataPin, HIGH);
      digitalWrite(dataPin, LOW);

      bit_value = 1;
    }
  else
    {
      digitalWrite(dataPin, HIGH);
      digitalWrite(dataPin, LOW);
      bit_value = 0;
    }

  // Get the delta
  t = micros();
  delta = t - t2;
  t2 = t;
  
  int i;
  
  // We now have N bits, we can work out how many by dividing the
  // delta by the bit time which is set up by the main code
  // Limit the number of bits we process so we don't have large startup delays
#define BIT_LIMIT 20
  
  if( delta > bit_period * BIT_LIMIT )
    {
      delta = bit_period * BIT_LIMIT;
    }
  else
    {
      number_of_bits = (delta+bit_period/2) / bit_period;
    }
  
  // Add some bits to the bit stream
  for(i=0; i<number_of_bits; i++)
    {
      // Shift one bit in to 'capture_bits' which is a window that we use to process
      // the bits as they arrive

      // Shift data in to the top bit position
      capture_bits = capture_bits >> 1;
      capture_bits += bit_value<<15;
      
      // One more bit
      capture_bits_len++;
      
      // The window looks like this (we use 8n1 format only)
      //
      //  SDDD DDDD DT.. ....
      //
      //  S: Start bit
      //  D:Data bit of 8 bits long byte
      //  T: Stop bit
      //  .: Following bits, we don't care about these at the moment
      //
      // Check for a frame, this is a start bit (1), 8 data bits and a stop bit
      // which is 1DDD DDDD D0xx xxxx
      // To check framing we use a mask of 0x8040, if the value we get is
      // 0x8000 then we have a start bit of 1 and a stop bit of 0, which is a frame
      
      if( (capture_bits & 0x8040) == 0x8000 )
	{
	  // We have a frame	  
	  // Extract the data byte from the window
	  
	  stored_bytes[bytecount++] = (capture_bits & 0x7f80) >> 7;

	  // We store the data bytes in a buffer and that has a maximum size. If we reach that
	  // size then we discard the data
	  
	  if ( bytecount >= MAX_BYTES )
	    {
	      bytecount = MAX_BYTES -1;
	    }

	  // Every 500 bytes we display a status message
	  if( (bytecount % 500) == 0 )
	    {
	      Serial.print(bytecount);
	      Serial.println(" bytes");
	    }
#if 1
	  //Serial.print(".");
#else
	  
	  Serial.print(capture_bits, HEX);

	  Serial.print(" ");
	  Serial.print(stored_bytes[bytecount-1], HEX);

	  
	  if ( (bytecount % 16) == 0)
	    {
	      Serial.println("");
	    }
#endif

	  // Blank out the byte we just received by replacing the bits of that frame with the
	  // start bit line state. As the stop bit value is 0 the data byte detetion code above will not
	  // see a data byte until the stop bit is received, which is a 0
	  capture_bits = 0xffff;
	}
      
      // Signal we have a bit or bits
      got_bit = 1;
    }

  // Debug
  digitalWrite(statPin, LOW);
}
