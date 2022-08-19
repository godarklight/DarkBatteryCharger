int relayPin = 11;
int batteryPin = A0;
int average[16];
int averageWritePos = 0;
unsigned long lastVoltageTime = 0;
unsigned long lastTime = 0;
int lastActTime = 0;
bool charging = true;
byte command[64];
int commandWritePos = 0;
int offOverride = 0;
int onOverride = 0;
float lowSetPoint = 10.8;
float highSetPoint = 14;

void setup() {
  //Setup Serial
  Serial.begin(9600);
  //Setup Relay
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  //Setup Battery
  analogReference(DEFAULT);
  for (int i = 0; i < 16; i++)
  {
    average[i] = 0;
  }
}

void write_serial(String input)
{
  int writeAvailable = Serial.availableForWrite();
  if (input.length() + 1 >= writeAvailable)
  {
    return;
  }
  Serial.println(input);
}

void charge_off()
{
  charging = false;
  digitalWrite(relayPin, HIGH);
  lastActTime = 0;
}

void charge_on()
{
  charging = true;
  digitalWrite(relayPin, LOW);
  lastActTime = 0;
}

void process_command()
{
  String command_string = String((char*)command);
  int command_length = command_string.length();
  
  //State control
  if (command_string.startsWith("OFF"))
  {
    charge_off();
    onOverride = 0;
    if (command_length > 4)
    {
      String offText = command_string.substring(4);
      offOverride = offText.toInt();
      write_serial("UPDATE: OFF " + String(offOverride));
    }
    else
    {
      write_serial("UPDATE: OFF -1");
      offOverride = -1;
    }
  }
  
  if (command_string.startsWith("ON"))
  {
    charge_on();
    offOverride = 0;
    if (command_length > 3)
    {
      String onText = command_string.substring(3);
      onOverride = onText.toInt();
      write_serial("UPDATE: ON " + String(onOverride));
    }
    else
    {
      write_serial("UPDATE: ON -1");
      onOverride = -1;
    }
  }
  
  if (command_string == "AUTO")
  {
    write_serial("UPDATE: AUTO");
    offOverride = 0;
    onOverride = 0;
  }
  
  //Voltage control
  if (command_string.startsWith("LOW"))
  {
    if (command_length > 4)
    {
      String parseText = command_string.substring(4);
      lowSetPoint = parseText.toFloat();
      write_serial("UPDATE: LOW " + String(lowSetPoint));
    }
  }

  //Voltage control
  if (command_string.startsWith("HIGH"))
  {
    if (command_length > 5)
    {
      String parseText = command_string.substring(5);
      highSetPoint = parseText.toFloat();
      write_serial("UPDATE: HIGH " + String(highSetPoint));
    }
  }

}

void loop() {
  unsigned long currentTime = millis();
  
  //10Hz voltage update
  if (currentTime - lastVoltageTime > 100)
  {
    lastVoltageTime = currentTime;
    average[averageWritePos] = analogRead(batteryPin);
    averageWritePos = (averageWritePos + 1) % 16;
  }

  //1Hz voltage checking
  if (currentTime - lastTime > 1000)
  {
    lastTime = currentTime;
    lastActTime++;
    float battery_voltage = get_voltage();
    write_serial(String(battery_voltage, 3));

    //Decrement overrides
    if (offOverride > 0)
    {
      offOverride--;
      if (offOverride == 0)
      {
        write_serial("UPDATE: OFF TO AUTO");
      }
    }
    if (onOverride > 0)
    {
      onOverride--;
      if(onOverride == 0)
      {
        write_serial("UPDATE: ON TO AUTO");  
      }      
    }

    if (lastActTime > 30)
    {
      //Turn off auto
      if (onOverride == 0 && charging && battery_voltage > highSetPoint)
      {
        write_serial("UPDATE: RELAY OFF " + String(lastActTime));
        charge_off();
      }
      //Turn on auto
      if (offOverride == 0 && !charging && battery_voltage < lowSetPoint)
      {
        write_serial("UPDATE: RELAY ON " + String(lastActTime));
        charge_on();
      }
    }
    //Never let the battery get under 10.5V
    if (!charging && battery_voltage < 10.5)
    {
      write_serial("UPDATE: RELAY ON " + String(lastActTime));
      charge_on();
    }
  }
  //Serial control
  int serialAvailable = Serial.available();
  while (serialAvailable > 0)
  {
    int receiveByte = Serial.read();
    command[commandWritePos] = receiveByte;
    commandWritePos++;
    serialAvailable--;
    if (receiveByte == 10)
    {
      command[commandWritePos - 1] = 0;
      process_command();
      commandWritePos = 0;
    }
    //Don't overrun if we get too long a command
    if (commandWritePos == 64)
    {
      commandWritePos = 0;
    }
  }
  //Rollover millis
  if (lastTime > currentTime)
  {
    lastTime = currentTime;
  }
}

float get_voltage()
{
  int accumulator = 0;
  for (int i = 0; i < 16; i++)
  {
    accumulator = accumulator + average[i];
  }
  float adcPercent = accumulator / (16.0 * 1024.0);
  float adcVoltage = adcPercent * 5.0;
  //12v <-9.9k ohm-> A0 <-> 3.3k ohm <-> GND
  //13.2 / 3.3 is our multiplier, with 5V reference.
  float fudge = (13.8 / 14.0);
  float resistorDivide = (13.2 / 3.3);
  return adcVoltage * resistorDivide * fudge;
}
