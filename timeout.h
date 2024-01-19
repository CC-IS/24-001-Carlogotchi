#ifndef TIMEOUT
#define TIMEOUT

////////////////////////////////////////////////////////////////////////////
// Non-blocking timing functions for arduino
////////////////////////////////////////////////////////////////////////////

#include "arduino.h"

template <typename CT, typename ... A> class function
: public function<decltype(&CT::operator())()> {};

class timeOutFunc{
  public:
    virtual void operator()();
};

template <typename C> class function<C>: public timeOutFunc {
private:
    C mObject;

public:
    function(const C & obj) : mObject(obj) {}

    void operator()() {
        this->mObject.operator()();
    }
};

class TimeOut {
public:
  bool running;
  unsigned long timer;
  void (*voidCB)();
  timeOutFunc* callback;
  bool lambda;

  TimeOut(){
    timer = 0;
    running = false;
    lambda = false;
  }

  bool active(){
    return timer > millis();
  }

  template<typename C>
  void set(const C & CB, unsigned int time){
    running = true;
    timer = millis() + time;
    callback = new function<C>(CB);
    lambda = true;
  }

  void set(void (*CB)(), unsigned int time){
    running = true;
    timer = millis() + time;
    voidCB = CB;
  }
  
  void clear(){
    running = false;
  }
  
  bool idle(){
    if(timer < millis() && running){
      if(lambda){
        (*callback)();
        delete callback;
      } else voidCB();
      lambda = false;
      running = false;
      return true;
    }

    return false;
  }
};

TimeOut timeOuts[10];

template<typename C>
int setTimeout(const C & CB, unsigned int amt, int data = 0){
  int ret = -1;
  for(int i =0; i< 10; i++){
    if(!timeOuts[i].running){
      timeOuts[i].set(CB,amt); 
      ret = i;
      break;
    }
  }

  return ret;
}

void idleTimers(){
  for(int i =0; i< 10; i++){
    timeOuts[i].idle();
    
  }
}

void clearTimeout(int which){
  if(which >=0) timeOuts[which].running = false;
}

#endif
