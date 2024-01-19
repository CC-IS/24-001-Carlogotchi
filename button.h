#ifndef BUTTON
#define BUTTON

////////////////////////////////////////////////////
// Button class, built to debounce buttons and simplfy handling of presses
// All actions go into handlers when you set up the button, and only "idle" is called in the loop.
////////////////////////////////////////////////////

#include "arduino.h"

//stuff to make lambda functions storable.
namespace DigitalButton {
  
  template <typename CT, typename ... A> class fxn
  : public fxn<decltype(&CT::operator())()> {};
  
  class dButFunc{
    public:
      virtual void operator()(int);
  };
  
  template <typename C> class fxn<C>: public dButFunc {
  private:
      C mObject;
  
  public:
      fxn(const C & obj) : mObject(obj) {}
  
      void operator()(int a) {
          this->mObject(a);
      }
  };

}

class Button {
public:
  int state;
  bool lastFired;
  unsigned long debounceTimer;
  int debounce;
  int pin;
  
  void (*callback)(int state);
  DigitalButton::dButFunc* pressCB;

  Button(){
  }

  void setup(int p, void (* cb)(int), unsigned long time = 20) {
    setCallback(cb);
    pin = p;
    pinMode(p, INPUT_PULLUP);
    debounceTimer = 0;
    debounce = time;
    lastFired  = true;
    state = -1;
  }

  template<typename C>
  void setup(int p, const C & cb, unsigned long time = 50) {
    setCallback(cb);
    pin = p;
    pinMode(p, INPUT_PULLUP);
    debounceTimer = 0;
    debounce = time;
    lastFired = state  = true;
  }

  template<typename C> 
  void setCallback( const C & cb){
    pressCB = new DigitalButton::fxn<C>(cb);
  }

  void setCallback( void (* cb)(int)){
    callback = cb;
  }
  
  void idle(){
    if(digitalRead(pin) != state){ //if the current state of the button doesn't match the last state of the button
      state = !state; //flip flop the last state
      debounceTimer = millis() + debounce; // and set the debounceTimer to the current time plus the debounce time
    }

    if(debounceTimer < millis() && lastFired != state){ //if the debounceTimer is less than the current time, and the state isn't the same as the last time the callback fired,
      lastFired  = state; // set teh lastFired state to the current state
      //and fire the callbacks, with state inverted, so state is whether or not the button is pressed.
      if(pressCB) (*pressCB)(!state);
      else if(callback) callback(!state);
    }
  }
};

#endif