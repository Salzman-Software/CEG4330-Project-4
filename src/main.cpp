#include <Arduino.h>

const int PWM_BITS = 8; // duty range 0-255
const int PWM_PIN = 2;  // onboard LED
const int IN_PIN = 4;   // connect to PWM_PIN with a jumper wire
double pressFrequency = 0.0;
double dutyCycle = 0.0;

uint8_t duty = 127;
uint16_t freq = 5000;

volatile unsigned long lastRiseTime = 0;
volatile unsigned long period = 0;
volatile unsigned long highTime = 0;
volatile unsigned long lastEdgeTime = 0;

#define PWM_ISR_ATTR IRAM_ATTR
void PWM_ISR_ATTR incomingPWMISR()
{
  unsigned long currentTime = micros();

  if (digitalRead(IN_PIN) == HIGH)
  {
    // Rising edge detected.

    if (lastRiseTime != 0)
    {
      // Time between two consecutive rising edges.
      period = currentTime - lastRiseTime;
    }

    lastRiseTime = currentTime;
  }
  else
  {
    // Falling edge detected.

    if (lastRiseTime != 0)
    {
      // Time from the rising edge to the falling edge.
      highTime = currentTime - lastRiseTime;
    }
  }

  lastEdgeTime = currentTime;
}

void setup()
{
  // Setup serial monitor
  Serial.begin(115200);

  // Setup GPIO and interrupt
  ledcAttach(PWM_PIN, freq, PWM_BITS);
  ledcWrite(PWM_PIN, duty);
  pinMode(IN_PIN, INPUT);

  attachInterrupt(
      digitalPinToInterrupt(IN_PIN),
      incomingPWMISR,
      CHANGE);
}

void changeDutyCycle(long value)
{
  // limit from 0 to 255
  if (value < 0)
  {
    value = 0;
  }
  if (value > 255)
  {
    value = 255;
  }
  // change duty cycle
  duty = value;
  Serial.print("Set duty: ");
  Serial.println("d" + String(duty));
  ledcWrite(PWM_PIN, duty);
}

void changeFrequency(int value)
{
  // Limit the value to be in the range of 100 to 10000
  if (value < 100)
  {
    value = 100;
  }
  if (value > 65535)
  {
    value = 65535;
  }
  // Change the frequency
  freq = value;
  Serial.print("Set freq: ");
  Serial.println("f" + String(freq));
  ledcWrite(PWM_PIN, duty);
  ledcChangeFrequency(PWM_PIN, freq, 8);
}

void getSerialInput()
{
  if (!Serial.available())
  {
    return;
  }
  String input = Serial.readStringUntil('\n');
  //
  if (input.startsWith("d"))
  {
    changeDutyCycle(input.substring(1).toInt());
  }
  if (input.startsWith("f"))
  {
    changeFrequency(input.substring(1).toInt());
  }
}

// write an ISR that measures the frequency and duty cycle of the incoming
// PWM signal. Then, implement the displayIncomingPWM() function
void displayIncomingPWM()
{
  static unsigned long lastPrintTime = 0;

  unsigned long currentTime = millis();

  // Print once per second.
  if (currentTime - lastPrintTime < 1000)
  {
    return;
  }

  lastPrintTime = currentTime;

  // turn off interrupts so it doesnt fuck shit up while measurements are being recorded. I was hunting down this bug for ages.
  noInterrupts();

  unsigned long measuredPeriod = period;
  unsigned long measuredHighTime = highTime;
  unsigned long measuredLastEdge = lastEdgeTime;

  interrupts();

  // Is there actually even a PWM signal that I can detect?
  if (measuredPeriod == 0 ||
      micros() - measuredLastEdge > 200000UL)
  {
    Serial.println("ESP32 Incoming PWM: No measurable signal");
    return;
  }

  // Can't seem to find one :v
  if (measuredHighTime > measuredPeriod)
  {
    Serial.println("ESP32 Incoming PWM: Invalid measurement");
    return;
  }

  // Calculate frequency in Hz.
  double frequency = 1000000.0 / measuredPeriod;

  // Calculate duty cycle as a percentage.
  double dutyCycle = 100.0 * measuredHighTime / measuredPeriod;

  // Display measurements.
  Serial.print("ESP32 Incoming PWM Frequency: ");
  Serial.print(frequency, 1);
  Serial.println(" Hz");

  Serial.print("ESP32 Incoming PWM Duty Cycle: ");
  Serial.print(dutyCycle, 1);
  Serial.println("%");

  Serial.println();
}

void loop()
{
  // Get the input from the serial monitor for changing frequency and duty cycle
  getSerialInput();

  // Display the incoming PWM signal's duty cycle and frequency
  displayIncomingPWM();
}
