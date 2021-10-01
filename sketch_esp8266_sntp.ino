#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define RX_BUF_SIZE             128
#define WIFI_CONN_CHK_INTERVAL  500 //msec
#define WIFI_CONN_RETRY_COUNT    10
#define UDP_LOCAL_PORT         1234
#define SNTP_PKT_LEN             48
#define SNTP_RCV_TIMEOUT       5000 //msec

char rx_buf[RX_BUF_SIZE];
int rx_buf_count = 0;
WiFiUDP udp;
byte sntp_pkt_buf[SNTP_PKT_LEN];
char msg_buf[32];

// debug
//int32_t date_time_sec0;
//int32_t date_time_usec0;
//int32_t comm_delay_usec0;
//int32_t proc_delay0;

void setup()
{
  Serial.begin(115200);
}

void output_datetime(uint32_t date_time_sec, uint32_t date_time_usec, int32_t proc_start_tick)
{
  const uint32_t days_4years[] = 
  {   0,   31,   60,   91,  121,  152,  182,  213,  244,  274,  305,  335, \
    366,  397,  425,  456,  486,  517,  547,  578,  609,  639,  670,  700, \
    731,  762,  790,  821,  851,  882,  912,  943,  974, 1004, 1035, 1065, \
   1096, 1127, 1155, 1186, 1216, 1247, 1277, 1308, 1339, 1369, 1400, 1430};

  //date_time_sec0 = date_time_sec;
  //date_time_usec0 = date_time_usec;

  date_time_sec += 9 * 3600 + 2; // JST time offset + 2 sec
  uint32_t days_from_1904 = (date_time_sec / 86400) - 365 * 4;
  uint32_t year4 = days_from_1904 / (365 * 4 + 1);
  uint32_t rest_days = days_from_1904 - year4 * (365 * 4 + 1);

  uint32_t i;
  for(i = (sizeof(days_4years) / sizeof(days_4years[0])) - 1; i > 0; i--)
  {
    if(rest_days >= days_4years[i])
      break;
  }
  uint32_t year = 1904 + year4 * 4 + int(i / 12) - 2000;
  uint32_t month = i % 12 + 1;
  uint32_t day = rest_days - days_4years[i] + 1;
  uint32_t day_of_week = (days_from_1904 + 5) % 7;
  uint32_t second = date_time_sec % 60;
  date_time_sec /= 60;
  uint32_t minute = date_time_sec % 60;
  date_time_sec /= 60;
  uint32_t hour = date_time_sec % 24;

  // output message length is 23 bytes (excluding null terminator)
  // ex.: OK_21-10-01-5T15:43:00\r
  sprintf(msg_buf, "OK_%2.2d-%2.2d-%2.2d-%dT%2.2d:%2.2d:%2.2d\r",
    year, month, day, day_of_week, hour, minute, second);

  int32_t proc_delay = (int32_t)(micros() - proc_start_tick);
  //proc_delay0 = proc_delay;
  // 23 bytes * 10 = 230 bits, 230 / 115200 bps = 1.996 msec
  delay((2000000 - date_time_usec - proc_delay - 1996) / 1000);

  Serial.print(msg_buf);
  
  //sprintf(msg_buf, "date_time_sec0=%u\r", date_time_sec0); Serial.print(msg_buf);
  //sprintf(msg_buf, "date_time_usec0=%d\r", date_time_usec0); Serial.print(msg_buf);
  //sprintf(msg_buf, "i=%d\r", i); Serial.print(msg_buf);
  //sprintf(msg_buf, "comm_delay_usec0=%d\r", comm_delay_usec0); Serial.print(msg_buf);
  //sprintf(msg_buf, "proc_delay0=%d\r", proc_delay0); Serial.print(msg_buf);
  //Serial.print("SNTP receive packet\r");
  //for(i = 0; i < SNTP_PKT_LEN; i++)
  //{
  //  sprintf(msg_buf, " %2.2x", sntp_pkt_buf[i]); Serial.print(msg_buf);
  //  if((i & 0xf) == 0xf) Serial.print("\r");    
  //}
}

void get_time(char* host)
{
  if(WiFi.status() != WL_CONNECTED)
  {
    Serial.print("ERR_WIFI_NOT_CONNECTED\r");
    return;
  }

  if(udp.begin(UDP_LOCAL_PORT) != 1)
  {
    Serial.print("ERR_BEGIN_UDP_FAILED\r");
    return;    
  }

  memset(sntp_pkt_buf, 0, SNTP_PKT_LEN);
  sntp_pkt_buf[0] = 0b00100011;   // LI = 0, Version = 4, Mode = 3
  sntp_pkt_buf[1] = 0;     // Stratum, or type of clock
  sntp_pkt_buf[2] = 4;     // Polling Interval 2 ^ 4 = 16 sec
  sntp_pkt_buf[3] = 0xEC;  // Peer Clock Precision

  udp.flush();
  if(udp.beginPacket(host, 123) != 1)
  {
    Serial.print("ERR_UNREACHABLE_HOST ");
    Serial.print(host);
    Serial.print("\r");
    return;    
  }
  udp.write(sntp_pkt_buf, SNTP_PKT_LEN);
  uint32_t transmit_tick = micros();
  udp.endPacket();

  byte sntp_rcv_poll_count = 0;
  int rcv_pkt_size = 0;
  uint32_t rcv_start_tick = millis();
  while(rcv_pkt_size < SNTP_PKT_LEN && (millis() - rcv_start_tick) < SNTP_RCV_TIMEOUT)
  {
    rcv_pkt_size = udp.parsePacket();
  }
  if(rcv_pkt_size < SNTP_PKT_LEN)
  {
    Serial.print("ERR_RECEIVE_UDP_PKT_TIMEOUT\r");
    udp.stop();
    return;    
  }

  udp.read(sntp_pkt_buf, SNTP_PKT_LEN);
  uint32_t proc_start_tick = micros();
  int32_t comm_delay_usec = (int32_t)(micros() - transmit_tick); // to take care overflow, calculation should be in uint32_t.
      
  uint32_t server_trans_ts_sec =
    ((uint32_t)sntp_pkt_buf[40] << 24) | ((uint32_t)sntp_pkt_buf[41] << 16) |
    ((uint32_t)sntp_pkt_buf[42] <<  8) | (uint32_t)sntp_pkt_buf[43];
  uint32_t server_trans_ts_frac =
    ((uint32_t)sntp_pkt_buf[44] << 24) | ((uint32_t)sntp_pkt_buf[45] << 16) |
    ((uint32_t)sntp_pkt_buf[46] <<  8) | ((uint32_t)sntp_pkt_buf[47]);
  // If we think (server_trans_ts_frac >> 12) is integer the value, it is equals to server_trans_ts_frac << 20 bit.
  uint32_t server_trans_ts_frac_usec = (int32_t)(((uint64_t)(server_trans_ts_frac >> 12) * 1000000) >> 20);

  //comm_delay_usec0 = comm_delay_usec;
  server_trans_ts_frac_usec += comm_delay_usec / 2;
  while(server_trans_ts_frac_usec > 1000000)
  {
    server_trans_ts_sec++;
    server_trans_ts_frac_usec -= 1000000;
  }
  output_datetime(server_trans_ts_sec, server_trans_ts_frac_usec, proc_start_tick);
  udp.stop();
}

void loop()
{
  if (Serial.available() > 0)
  {
    // If the buffer is full, clear the buffer
    if(rx_buf_count >= RX_BUF_SIZE)
    {
      rx_buf_count = 0;
    }

    rx_buf[rx_buf_count++] = Serial.read();
    if(rx_buf_count > 0 && rx_buf[rx_buf_count - 1] == '\r')
    {
      if(rx_buf_count > 10 && strncmp(rx_buf, "wifibegin=", 10) == 0)
      {
        rx_buf[rx_buf_count - 1] = 0;
        char* ssid = rx_buf + 10;
        char* password = strchr(ssid, ',');
        if(password == NULL)
        {
          Serial.print("ERR_INVALID_SSID_PASSWORD\r");
        }
        else
        {
          int retry_count = 0;
          *password = 0;
          password++;
          WiFi.begin(ssid, password);
          
          while(retry_count < WIFI_CONN_RETRY_COUNT && WiFi.status() != WL_CONNECTED)
          {
            Serial.print(".");
            delay(WIFI_CONN_CHK_INTERVAL);
          }
          Serial.print("\r");
          if(WiFi.status() == WL_CONNECTED)
          {
            Serial.print("OK\r");
          }
          else
          {
            Serial.print("ERR_CONNECTION_FAIL\r");
          }
        }        
      }
      else if(rx_buf_count > 5 && strncmp(rx_buf, "time=", 5) == 0)
      {
        rx_buf[rx_buf_count - 1] = 0;
        get_time(rx_buf + 5);
      }
      else if(rx_buf_count > 6 && strncmp(rx_buf, "wifist", 6) == 0)
      {
        IPAddress ip = WiFi.localIP();
        sprintf(msg_buf, "status:%d localIP:%d.%d.%d.%d\r",
          WiFi.status(), ip[0], ip[1], ip[2], ip[3]);
        Serial.print(msg_buf);
      }
      else
      {
        Serial.print("ERR_UNKNOWN_COMMAND\r");
      }

      rx_buf_count = 0;
    }
  }
}
