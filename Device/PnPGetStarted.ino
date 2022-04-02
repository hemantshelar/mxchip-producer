// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. 
// To get started please visit https://microsoft.github.io/azure-iot-developer-kit/docs/projects/connect-iot-hub?utm_source=ArduinoExtension&utm_medium=ReleaseNote&utm_campaign=VSCode
#include "AZ3166WiFi.h"
#include "AzureIotHub.h"
#include "DevKitMQTTClient.h"

#include "config.h"
#include "utility.h"
#include "SystemTickCounter.h"

#include "EEPROMInterface.h"
#include "SerialLog.h"

static bool hasWifi = false;
int messageCount = 1;
int sentMessageCount = 0;
static bool messageSending = true;
static uint64_t send_interval_ms;
static const char model_id[] = "dtmi:com:mxchip:mxchip_iot_devkit:example:PnPGetStarted;1";
static const char sampleProperties[] = "{\"sample\": \"DevKit-PnP-GetStarted\" }";
static const char responseMessageFormat[] = "{\"message\": \"%s\" }";
static const char* successResponse = "Successfully invoke device method";
static const char* failResponse = "No method found";

static float temperature;
static float humidity;
static float pressure;

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities
static void InitWifi()
{
  Screen.print(2, "Connecting...");
  
  if (WiFi.begin() == WL_CONNECTED)
  {
    IPAddress ip = WiFi.localIP();
    Screen.print(1, ip.get_address());
    hasWifi = true;
    Screen.print(2, "Running... \r\n");
  }
  else
  {
    hasWifi = false;
    Screen.print(1, "No Wi-Fi\r\n ");
  }
}

static void BuildCommandResponse(unsigned char** response, int* response_size, const char* message) 
{
  int responseBuilderSize = 0;
  unsigned char* responseBuilder = NULL;
  int result = 200;

  if ((responseBuilderSize = snprintf(NULL, 0, responseMessageFormat, message)) < 0)
  {
    LogError("snprintf to determine string length for command response failed");
  }
  else if ((responseBuilder = (unsigned char*)calloc(1, responseBuilderSize + 1)) == NULL)
  {
    LogError("Unable to allocate %lu bytes", (unsigned long)(responseBuilderSize + 1));
  }
  else if ((responseBuilderSize = snprintf((char*)responseBuilder, responseBuilderSize + 1, responseMessageFormat, message)) < 0)
  {
    LogError("snprintf to output buffer for command response");
  }

  *response = responseBuilder;
  *response_size = responseBuilderSize;
  LogInfo("Response=<%s>", (const char*)responseBuilder);
}

static void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result)
{
  if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
  {
    blinkSendConfirmation();
    sentMessageCount++;
  }

  Screen.print(1, "> IoT Hub");
  char line1[20];
  sprintf(line1, "Count: %d/%d",sentMessageCount, messageCount); 
  Screen.print(2, line1);

  char line2[20];
  sprintf(line2, "T:%.2f H:%.2f", temperature, humidity);
  Screen.print(3, line2);
  messageCount++;
}

static void MessageCallback(const char* payLoad, int size)
{
  blinkLED();
  Screen.print(1, payLoad, true);
}

static void DeviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, int size)
{
  char *temp = (char *)malloc(size + 1);
  if (temp == NULL)
  {
    return;
  }
  memcpy(temp, payLoad, size);
  temp[size] = '\0';
  parseTwinMessage(updateState, temp);
  free(temp);
}

static int  DeviceMethodCallback(const char *methodName, const unsigned char *payload, int size, unsigned char **response, int *response_size)
{
  LogInfo("Try to invoke method %s", methodName);
  int result = 200;

  if (strcmp(methodName, "start") == 0)
  {
    LogInfo("Start sending temperature, humidity and pressure data");
    messageSending = true;
  }
  else if (strcmp(methodName, "stop") == 0)
  {
    LogInfo("Stop sending temperature, humidity and pressure data");
    messageSending = false;
  }
  else
  {
    LogInfo("No method %s found", methodName);
    BuildCommandResponse(response, response_size, failResponse);
    result = 404;
  }
  BuildCommandResponse(response, response_size, successResponse);
  return result;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Arduino sketch
void setup()
{
  Screen.init();
  Screen.print(0, "IoT DevKit");
  Screen.print(2, "Initializing...");
  
  Screen.print(3, " > Serial");
  Serial.begin(115200);

  // Initialize the WiFi module
  Screen.print(3, " > WiFi");
  hasWifi = false;
  InitWifi();
  if (!hasWifi)
  {
    return;
  }

  LogTrace("HappyPathSetup", NULL);

  Screen.print(3, " > Sensors");
  SensorInit();

  Screen.print(3, " > IoT Hub");

  char connString[] = "HostName=name.net;DeviceId=mxchip;SharedAccessKey=yoursharedaccesskey";
                       
  EEPROMInterface eeprom;
  int ret = eeprom.write((uint8_t*)connString, 
                           strlen(connString), 
                           AZ_IOT_HUB_ZONE_IDX);
  char buff10[100];
  sprintf(buff10,"@Ret=%d",ret);
  Screen.print(3,buff10);
  delay(4000);

  DevKitMQTTClient_SetOption(OPTION_MINI_SOLUTION_NAME, "DevKit-PnP-GetStarted");
  DevKitMQTTClient_Init(true, false, model_id);
  DevKitMQTTClient_SetSendConfirmationCallback(SendConfirmationCallback);
  DevKitMQTTClient_SetMessageCallback(MessageCallback);
  DevKitMQTTClient_SetDeviceTwinCallback(DeviceTwinCallback);
  DevKitMQTTClient_SetDeviceMethodCallback(DeviceMethodCallback);

  EVENT_INSTANCE* message = DevKitMQTTClient_Event_Generate(sampleProperties, STATE);
  DevKitMQTTClient_SendEventInstance(message);

  send_interval_ms = SystemTickCounterRead();
}

void loop()
{
  Screen.print(1,"hemant");  
  if (hasWifi)
  {
    if (messageSending && 
        (int)(SystemTickCounterRead() - send_interval_ms) >= getInterval())
    {
      // Send teperature data
      char messagePayload[MESSAGE_MAX_LEN];
      char buff[100];

      readMessage(messageCount, messagePayload, &temperature, &humidity, &pressure);
      sprintf(buff,"T: %f",temperature);
      Screen.print(1,buff);
      EVENT_INSTANCE* message = DevKitMQTTClient_Event_Generate(messagePayload, MESSAGE);
      bool sent = DevKitMQTTClient_SendEventInstance(message);
      if ( sent == true ){
        Screen.print(1,"Message sent");
      }else{
        Screen.print(1,"Message not sent");
      }
      
      send_interval_ms = SystemTickCounterRead();
    }
    else
    {
      DevKitMQTTClient_Check();
    }
  }
  delay(1000*60);
}
