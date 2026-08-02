#include <Keyboard.h>
#define KEY_A 3
void setup() {
Keyboard.begin();
pinMode(KEY_A, INPUT_PULLUP);
}
void loop() {
if (digitalRead(KEY_A) == LOW) {
Keyboard.print("a");
delay(100);
}
}
