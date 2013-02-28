/*
  TurnOnMyPC Web Server
  lds133@gmail.com

 
  A simple web server that allows to control PC power switch and status LEDs using an Arduino Wiznet Ethernet shield. 
 
 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 * Power LED output attached to pin 5
 * HDD LED output attached to pin  4
 * Device status LED attached to pin A0
 * Power LED input attached to pin 6
 * HDD LED input attached to pin 3 
 * Relay coil to pin 7
 
 
 created 27 Feb 2013
 
 */

// For security reasons make this strings unique
#define SHORTPRESSPATTERN  "SHORTPRESS"
#define LONGPRESSPATTERN  "LONGPRESS"
#define INFOPATTERN  "INFO"

// Change this settings to fit your network configuration
#define DOMAIN "192.168.0.80"
#define IP1    192
#define IP2    168
#define IP3    0
#define IP4    80
#define PORT   8080








#include <SPI.h>
#include <Ethernet.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>

#define VERSION "1.0"

#define PIN_LED A0
#define PIN_RELAY 7
#define PIN_PWRIN 6 
#define PIN_PWROUT 5
#define PIN_HDDIN 3 
#define PIN_HDDOUT 4


#define FAVICONPATTERN  "favicon"
#define GETPATTERN  "GET /"

#define STATUS_ERROR 1
#define STATUS_SHORTPRESS 2
#define STATUS_LONGPRESS 3
#define STATUS_INFO 4
#define STATUS_FAVICON 5

#define LINEMAX 25

#define BUTTONSHORTPRESSTIME 1500
#define BUTTONLONGPRESSTIME  7000


#define HDDHISTORYSIZE  60
#define ONEHUNDREDPERSENT 100


PROGMEM  prog_uchar _favicon[] =
{
    0x00,0x00,0x01,0x00,0x01,0x00,0x10,0x10,0x02,0x00,0x00,0x00,0x01,0x00,0xB0,0x00,
    0x00,0x00,0x16,0x00,0x00,0x00,0x28,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x20,0x00,
    0x00,0x00,0x01,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xFF,0xFF,0xFF,0x00,0xFF,0xFF,0x00,0x00,0xFF,0xFF,0x00,0x00,0xF8,0x1F,
    0x00,0x00,0xF0,0x0F,0x00,0x00,0xE3,0xC7,0x00,0x00,0xC7,0xE3,0x00,0x00,0xCF,0xF3,
    0x00,0x00,0xCE,0x73,0x00,0x00,0xCE,0x73,0x00,0x00,0xCE,0x73,0x00,0x00,0xC6,0x63,
    0x00,0x00,0xE2,0x47,0x00,0x00,0xF2,0x4F,0x00,0x00,0xFE,0x7F,0x00,0x00,0xFE,0x7F,
    0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00
};
int _faviconsize;


typedef struct UpTimeStruct
{
    unsigned long uptime;
    unsigned long mstail;
    unsigned long lasttime;
} UPTIMESTRUCT;


byte _getpatternlen;
byte _mac[] = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };
IPAddress _ip(IP1,IP2,IP3,IP4);
EthernetServer _server(PORT);
char _line[LINEMAX];
int _pos;
unsigned long _time;
int _status;
byte _hddhistory[HDDHISTORYSIZE];
byte _hddhistorypos;
UPTIMESTRUCT _uptime;
UPTIMESTRUCT _ontime;
byte _lastpwrstatus;







void setup() 
{
  _getpatternlen = strlen(GETPATTERN);
  memset(&_uptime,0,sizeof(UpTimeStruct));
  memset(&_ontime,0,sizeof(UpTimeStruct));
  _lastpwrstatus = LOW;
  _faviconsize = sizeof(_favicon);
  
  Serial.begin(115200);
  
  Ethernet.begin(_mac, _ip);
  _server.begin();
  Serial.print("Version ");  
  Serial.println(VERSION);  
  Serial.print("Server is at ");
  Serial.println(Ethernet.localIP());
  Serial.println();
  
  pinMode(PIN_HDDIN, INPUT);
  pinMode(PIN_PWRIN, INPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_PWROUT, OUTPUT);
  pinMode(PIN_HDDOUT, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  
  StartupAnimation();
  
  digitalWrite(PIN_LED,HIGH);
  
  TimerSetup();
  
  wdt_enable(WDTO_8S);
}





void WriteResponce(int code,EthernetClient client)
{
    if (code == STATUS_ERROR)
    {  WriteError(client);
       return; 
    }

    if (code == STATUS_FAVICON)
    {  WriteFavicon(client);
       return; 
    }

    WriteHeader(client);
    
    if ((code == STATUS_SHORTPRESS) || (code == STATUS_LONGPRESS) || (code == STATUS_INFO))
    {    client.print("<meta http-equiv='refresh' content='30;URL=");
         WriteLink(client,INFOPATTERN);
         client.println("'>");
    }
    
    client.print("<h1>Internet Power Switch</h1>");
    client.print("<small>Version ");
    client.print(VERSION);
    client.print("</small>");
    client.println("</br>");    
    
    WriteUpTime(client,_uptime.uptime,"Up time");
    if (_lastpwrstatus == HIGH)
    {  WriteUpTime(client,_ontime.uptime,"ON time");
    } else
    {  WriteUpTime(client,_ontime.uptime,"OFF time");
    }

    switch (code)
    {
        case STATUS_SHORTPRESS:
           PressButton(BUTTONSHORTPRESSTIME);
           client.println("<p><b>Short press performed</b></p>");
           break;
        case STATUS_LONGPRESS:
           PressButton(BUTTONLONGPRESSTIME);
           client.println("<p><b>Long press performed</b></p>");
           break; 
        case STATUS_INFO:
           //client.println("<p>&nbsp;</p>");
           break;

        default:
            client.print("<p>Wrong status (");
            client.print(code);
            client.println(")</p>");            
            WriteFooter(client);
            break;
    }    
    
    client.println("<p>&nbsp;</p>"); 
    
    client.print("<p>Power led is ");
    client.print(digitalRead(PIN_PWRIN)==HIGH ? "<b>ON</b>" : "<b>OFF</b>");
    client.println("</p>");
  
    //client.print("<p>HDD led is ");
    //client.print(digitalRead(PIN_HDDIN)==HIGH ? "ON" : "OFF");
    //client.println("</p>");
    
    client.print("<p>HDD load  ");
    client.print(CalculateHddLoad());
    client.println("%</p>");    
 
    client.println("<p>&nbsp;</p>"); 
    WriteAHREF(client,INFOPATTERN,"Update");    
    client.println("<p></p>");    
    WriteAHREF(client,SHORTPRESSPATTERN,"Short press");
    client.println("<p></p>");    
    WriteAHREF(client,LONGPRESSPATTERN,"Long press");    
    
    WriteFooter(client);
}








void loop() 
{

  EthernetClient client = _server.available();

  UpdateUpTime(&_uptime);  
  UpdateUpTime(&_ontime);  
  
  wdt_reset();

  if (client) 
  {
    _status = STATUS_ERROR;
    digitalWrite(PIN_LED,LOW);
    Serial.println("new client");
    LineClear();
    boolean currentLineIsBlank = true;
    _time = millis();
    
    while (client.connected()) 
    {
        if (client.available()) 
        {   char c = client.read();
            if (c == '\n')
            {   if (currentLineIsBlank) 
                {   WriteResponce(_status,client);
                    break;
                }
                Serial.println((char*)_line);
                if (IsStartsWith(_line,GETPATTERN))
                {   _status = ProcessInput(_line+_getpatternlen);
                    Serial.print("Status: ");                  
                    Serial.println(_status);
                }
                LineClear();           
            } else
            {   LineAdd(c);  
            }
            if (c == '\n') 
            {   currentLineIsBlank = true;
            } else if (c != '\r') 
            {   currentLineIsBlank = false;
            }
        }
    }

    SafeDelay(1);
    client.stop();
    Serial.print("client disonnected (");
    Serial.print(millis() - _time);
    Serial.println(" ms)");
    digitalWrite(PIN_LED,HIGH);
  }
}







//   TIMER =====================================================================

void TimerSetup()
{
    cli();

    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;
    OCR1A = 7812;//3906 - 4 Hz , 7812 - 2Hz , 15624 - 1Hz
    TCCR1B |= (1 << WGM12);
    TCCR1B |= (1 << CS12) | (1 << CS10);  
    TIMSK1 |= (1 << OCIE1A);
    
    sei();
   
    memset(_hddhistory,0,HDDHISTORYSIZE);
    _hddhistorypos = 0;
    
    
}

ISR(TIMER1_COMPA_vect)
{

  if (digitalRead(PIN_HDDIN)==HIGH)
  {    _hddhistory[_hddhistorypos] = 1;
       digitalWrite(PIN_HDDOUT,HIGH);
  } else
  {    _hddhistory[_hddhistorypos] = 0;
       digitalWrite(PIN_HDDOUT,LOW);
  }  
  _hddhistorypos++;
  if (_hddhistorypos==HDDHISTORYSIZE) _hddhistorypos=0;
  
  byte pwrstatus = digitalRead(PIN_PWRIN);
  if (_lastpwrstatus!=pwrstatus)
  {  _ontime.uptime = 0;
     _lastpwrstatus=pwrstatus;
     digitalWrite(PIN_PWROUT,pwrstatus);
  }
  
  //Serial.print("#");

}


int CalculateHddLoad()//persent
{
    unsigned long sum=0;
    for(int i=0;i<HDDHISTORYSIZE;i++) sum+= _hddhistory[i];
    sum = (sum*ONEHUNDREDPERSENT) / HDDHISTORYSIZE;

    return sum;    
}

//   ============================================================================










int ProcessInput(char* cmd)
{
    if (IsStartsWith(cmd,SHORTPRESSPATTERN)) return STATUS_SHORTPRESS;
    if (IsStartsWith(cmd,LONGPRESSPATTERN))  return STATUS_LONGPRESS;   
    if (IsStartsWith(cmd,INFOPATTERN))       return STATUS_INFO;      
    if (IsStartsWith(cmd,FAVICONPATTERN))    return STATUS_FAVICON;      
    return STATUS_ERROR;
}




#define MILLI 1000
#define ULONGMAX 0xFFFFFFFF
#define MINUPTIME  60000
void UpdateUpTime(void* pupt)
{
    UPTIMESTRUCT* upt = (UPTIMESTRUCT*)pupt;
    
    unsigned long t = millis();
    if (t == upt->lasttime) return;
    unsigned long delta;
    delta = (t>upt->lasttime) ? (t - upt->lasttime) : (t + (ULONGMAX - upt->lasttime));
    delta += upt->mstail;
    if (delta < MINUPTIME) return;
        
    upt->lasttime  = t;
    upt->uptime += (delta / MILLI);
    upt->mstail = delta % MILLI;
}


unsigned long DAYS = 60*60*24;
unsigned long HOURS = 60*60;
unsigned long MINS  = 60;
void WriteUpTime(EthernetClient client,unsigned long uptime,char* text)
{
    unsigned long d,h,m;
    d = uptime / DAYS;
    h = (uptime - d*DAYS)/ HOURS;
    m = (uptime - d*DAYS - h*HOURS)/ MINS;
    client.print("<p>");
    client.print(text);
    client.print(" ");
    if (d!=0)
    {   client.print(d);
        client.print(" ");
    }
    if (h<10) client.print("0");
    client.print(h);
    client.print(":");
    if (m<10) client.print("0");
    client.print(m);
    client.println("</p>"); 
    client.print("<p>"); 
}

#define DELAYSTEP 1000
void SafeDelay(unsigned long ms)
{
    unsigned long t = ms;
    while(true)
    {
         wdt_reset();
         if (t>DELAYSTEP)
         {   delay(DELAYSTEP);
             t-=DELAYSTEP;
         } else
         {   delay(DELAYSTEP);
             break;
         }
    }
}


boolean IsStartsWith(char* str,char* pattern)
{
  int n = strlen(str);
  int m = strlen(pattern);
  int len = n>m ? m : n;
  if (len>LINEMAX) len = LINEMAX;
  
  for(int i=0;i<len;i++)
    if (*(str+i) != *(pattern+i)) return false;
    
  return true;
}





void StartupAnimation()
{
  for(int i=0;i<3;i++)
  {  digitalWrite(PIN_LED,HIGH);
     digitalWrite(PIN_PWROUT,HIGH);
     digitalWrite(PIN_HDDOUT,HIGH);
     SafeDelay(300);
     digitalWrite(PIN_LED,LOW);
     digitalWrite(PIN_PWROUT,LOW);
     digitalWrite(PIN_HDDOUT,LOW);     
     SafeDelay(300);
  }
}


void PressButton(int timeout)
{
    digitalWrite(PIN_RELAY,HIGH);
    SafeDelay(timeout);
    digitalWrite(PIN_RELAY,LOW);   
}




void LineClear()
{   memset(_line,0,LINEMAX);
    _pos=0;
}

void LineAdd(char c)
{   _line[_pos] = c;
    if (_pos<LINEMAX-1) _pos++;  
}


//  WRITE ============================================================================


void WriteHeader(EthernetClient client)
{
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connnection: close");
      client.println();
      client.println("<!DOCTYPE HTML>");
      client.println("<html>");

}

void WriteFooter(EthernetClient client)
{
      client.println("</html>");
}

void WriteError(EthernetClient client)
{                      
       client.println("HTTP/1.1 403 Forbidden");
       client.println("Connection: close");       
       client.println("Content-Type: text/html");
       client.println();
       client.println("<h2>You are not welcome here!</h2>");
}


void WriteLink(EthernetClient client,char* pattern)
{
    client.print("http://");
    client.print(DOMAIN);
    client.print(":");
    client.print(PORT);
    client.print("/");
    client.print(pattern);
}


void WriteAHREF(EthernetClient client,char* pattern,char* text)
{
    client.print("<a href='");
    WriteLink(client,pattern);
    client.print("'>");
    client.print(text);
    client.println("</a>");    
}


void WriteFavicon(EthernetClient client)
{
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: image/x-icon");
      client.println("Connnection: close");
      client.print("Content-length: ");
      client.println(_faviconsize);
      client.println();  
  
    for(int i=0;i<_faviconsize;i++)
        client.write( pgm_read_byte_near(_favicon + i)); 
}

//  ==================================================================================










