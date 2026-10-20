#include <Arduino.h>

const int PWM_PIN = 9; // OC1A, the Timer1 output pin
const int IN_PIN = 2;  // connect to PWM_PIN with a jumper wire

uint8_t duty = 127;
uint16_t freq = 5000;

volatile unsigned long lastRiseTime = 0;
volatile unsigned long period = 0;
volatile unsigned long highTime = 0;
volatile unsigned long lastEdgeTime = 0;

#define PWM_ISR_ATTR
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
      // Time from rising edge to falling edge.
      highTime = currentTime - lastRiseTime;
    }
  }

  lastEdgeTime = currentTime;
}

void setup()
{
  // Setup serial monitor
  Serial.begin(115200);

  // Setup Timer1 for PWM: phase correct, prescaler 8.
  // The timer counts 0 -> ICR1 -> 0 at 2 MHz, so one period is ICR1 microseconds.
  TCCR1A = _BV(COM1A1) | _BV(WGM11);        // PWM output on OC1A
  TCCR1B = _BV(WGM13) | _BV(CS11);          // top = ICR1, prescaler 8
  ICR1 = 1000000 / freq;                    // period (us)
  OCR1A = (unsigned long)duty * ICR1 / 255; // high time (us)

  // Setup GPIO and interrupt
  pinMode(PWM_PIN, OUTPUT);
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
  OCR1A = (unsigned long)duty * ICR1 / 255; // high time (us)
}

void changeFrequency(long value)
{
  // Limit the value to be in the range 100 ... 10000
  if (value < 100)
  {
    value = 100;
  }
  if (value > 10000)
  {
    value = 10000;
  }
  // Change the frequency
  freq = value;

  // Calculate the new Timer1 TOP value.
  uint16_t newTop = 1000000UL / freq;

  noInterrupts();

  // Avoid setting TOP below the current counter value.
  if (TCNT1 > newTop)
  {
    TCNT1 = 0;
  }

  // Update frequency.
  ICR1 = newTop;

  // Preserve the selected duty cycle.
  OCR1A = (unsigned long)duty * newTop / 255;

  interrupts();

  Serial.print("Set freq: ");
  Serial.println(freq);
}

void getSerialInput()
{
  if (!Serial.available())
  {
    return;
  }
  String input = Serial.readStringUntil('\n');
  // Parse input and change frequency or duty cycle
  if (input.startsWith("d"))
  {
    changeDutyCycle(input.substring(1).toInt());
  }
  if (input.startsWith("f"))
  {
    changeFrequency(input.substring(1).toInt());
  }
}

void displayIncomingPWM()
{
  static unsigned long lastPrintTime = 0;

  unsigned long currentTime = millis();

  // only print data once per second
  if (currentTime - lastPrintTime < 1000)
  {
    return;
  }

  lastPrintTime = currentTime;

  // turn off interrupts while reading volatile data to avoid writing between reading the firstr and last one
  noInterrupts();

  unsigned long measuredPeriod = period;
  unsigned long measuredHighTime = highTime;
  unsigned long measuredLastEdge = lastEdgeTime;

  interrupts();

  // Is there actually even a PWM signal that I can detect?
  if (measuredPeriod == 0 ||
      micros() - measuredLastEdge > 200000UL)
  {
    Serial.println("Incoming PWM: No measurable signal");
    return;
  }

  // Can't seem to find one :v
  if (measuredHighTime > measuredPeriod)
  {
    Serial.println("Incoming PWM: Invalid measurement");
    return;
  }

  // Calculate frequency in Hz
  double frequency = 1000000.0 / measuredPeriod;

  // Calculate duty cycle as a percentage
  double dutyCycle = 100.0 * measuredHighTime / measuredPeriod;

  // Display measurements.
  Serial.print("Incoming PWM Frequency: ");
  Serial.print(frequency, 1);
  Serial.println(" Hz");

  Serial.print("Incoming PWM Duty Cycle: ");
  Serial.print(dutyCycle, 1);
  Serial.println("%");

  Serial.println();
}

void loop()
{
  // Get input from serial monitor for changing frequency and duty cycle
  getSerialInput();

  // Display incoming PWM signal's duty cycle and frequency
  displayIncomingPWM();
}
