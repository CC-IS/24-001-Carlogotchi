#ifndef ASYNC_REQ
#define ASYNC_REQ

////////////////////////////////////////////////////
// This was originally an async request function, for a dual core processor
// it was truncated to be a blocking function on a single core; if it seems weird,
// that's why.
///////////////////////////////////////////////

#include <HTTPClient.h>
#include "time.h"
#include "ArduinoJson.h"

//templates for treating lambda functions as something we can store.
template <typename CT, typename ... A> class jfunction
: public jfunction<decltype(&CT::operator())()> {};

class jsonFunc{
  public:
    virtual void operator()(DynamicJsonDocument&);
};

template <typename C> class jfunction<C>: public jsonFunc {
private:
    C mObject;

public:
    jfunction(const C & obj) : mObject(obj) {}

    void operator()(DynamicJsonDocument& a) {
        this->mObject(a);
    }
};

//enum of call type.
enum callType{
  GET,
  POST,
  PUT
};

//class for handling making a webrequest and parsing the return as a JSON document
class JSONCall {
public:
  callType type;
  String url;
  String data;
  bool debug;
  time_t now;
  bool lambda;
  bool running;

  void (*voidCB)(DynamicJsonDocument&); //store a normal function as a callback
  jsonFunc* callback; //store a lambda func as a callback

  JSONCall(){
    now = 0;
    //expiry = 0;
    debug = false;
    type = GET;
    running = false;
  }

  //handle the backend http request
  void call(){
    Serial.println("made the call");
    HTTPClient req;
    req.begin(url);
    int httpCode = 0;
    Serial.println(url);

    //Make the request, depending on type; this is blocking.
    if(type == GET) httpCode = req.GET(), Serial.println("Getting");
    else if(type == POST) httpCode = req.POST(data);
    else if(type == PUT) httpCode = req.PUT(data);

    //handle the response
    if (httpCode > 0) { //doesn't handle any 400 codes, but oh well
      DynamicJsonDocument doc(2000);
      DeserializationError error = deserializeJson(doc, req.getString()); //parse the return string
  
      // Test if parsing succeeds.
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
      } else {
        if(!lambda) voidCB(doc); //make the callbacks, depending on if it's a lambda func or not.
        else (*callback)(doc);
        delete callback;
      }
    } else if(debug){
      Serial.println("HTTP Error:");
      Serial.println(httpCode);
    }
  }

  //funcs to set callbacks and start the call. 
  template<typename C>
  void request(String URL, const C & CB, String DATA=""){
    if(DATA.length()) type = POST;
    else type = GET;
    url = URL;
    callback = new jfunction<C>(CB);
    running = true;
    lambda = true;
    //startRequest();
    call();
  }

  void request(String URL, void (*CB)(DynamicJsonDocument&), String DATA = ""){
    if(DATA.length()) type = POST;
    else type = GET;
    url = URL;
    running = true;
    voidCB = CB;
    call();
    //startRequest();
  }
} web;



#endif