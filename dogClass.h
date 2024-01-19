#ifndef DOG_OBJ
#define DOG_OBJ

////////////////////////////////////////////////////////////////////////
// Class to handle all of the dog functions. 
////////////////////////////////////////////////////////////////////////

#include "timeout.h"

//enums for the dog actions
enum Action {
  WALK_DWN,
  WALK_RIGHT,
  WALK_UP,
  WALK_LEFT,
  SIT_FOR,
  STARTSIT,
  SIT_RIGHT,
  SLEEP,
  RUN
};

// enum for the dog moods
enum Emote {
  HEART,
  POOP,
  MAD,
  HAPPY,
  FOOD,
  PLAY,
  SLEEPY,
  GRUMPY
};

//class to store the locations of the poops.
class Poop {
  public:
  int x,y;
  Poop(){
    x=0;
    y=0;
  }
  void move(int X, int Y){
    x=X;
    y=Y;
  }
};

//vars for storing info about poop locations.
const int maxPoops = 5;;
Poop poops[maxPoops];
int curPoops = 0;


//class containing all the info about the dog; apologies ahead of time.
// I wrote this in like two days, and didn't go back to clean anything up
class Dog{
  //declare all the things as public, because who cares.
  public:
  int x,y,dx,dy, xdest, ydest;  //variables to store current location, current velocity, and destination coordinates
  int xmin, ymin, xmax, ymax; // directional boundries
  int counter;  //frame counter variable, to store which sprite is shown
  int maxCounts; //max steps per animation
  Action act; // holds the current action of the dog
  Emote state; // holds the current emotion
  bool once, bypass, bShuffling; //a variety of bool declarations
  timeOutFunc* sitCB; //callback for what to do once the dog sits.
  timeOutFunc* gotoCB; //callback storage for what to do once the dog gets to it's destination
  timeOutFunc* drawCB; //callback for rendering
  int stepTime = 250; //default time in milliseconds between animation steps
  unsigned long lastStep = 0; // stores the time the last step was taken
  unsigned long hngrTmr, playTmr, trdTmr, poopTmr, noteTmr,changeTmr, shuffleTmr; //a bunch of time storage variables
  bool speak, freeMove, goX, goY, left,gotoFlag, poopFlag, busy; // more bool flags
  Emote note = HEART; // stores the emotions stored in the speech bubble

  //stores the emotional states of the dog
  char hunger,happiness,tiredness;
//initialize the dog object
  Dog(){
    x=-128;
    y=60;
    dx=15;
    dy=0;
    counter = 0;
    maxCounts = 4;
    once = bypass = false;
    act = WALK_RIGHT; //walk right by default
    state=HAPPY; // happy by default
    hunger = 0;
    happiness = 255;
    tiredness = 0;
    hngrTmr=playTmr=trdTmr=poopTmr=0;
    drawCB = NULL;
    xmin=-128;
    ymin = 60;
    xmax = 240;
    ymax = 100;
    xdest = ydest = 0;
    freeMove = true;
    goX = goY = false;
    left = gotoFlag = poopFlag = busy = false;
    bShuffling = false;
  }

  //functions to store lambda functions as callbacks.
  template<typename C>
  void setSitCB(const C & CB){
    sitCB = new function<C>(CB);
  }

  template<typename C>
  void setGotoCB(const C & CB){
    gotoCB = new function<C>(CB);
  }

  template<typename C>
  void setDrawCB(const C & CB){
    drawCB = new function<C>(CB);
  }

  // check the step timer to see if we're ready to take the next step.
  bool stepReady(){
    return millis()>lastStep + stepTime;
  }

  //function to actually move the dog
  void step(){
    if(!bypass){ //if we are allowing the dog to be moved (ie, haven't set the bypass flag)
      lastStep = millis(); //record the current time as the time the last step was taken
      x+=dx; //increment x and y by dx and dy respectively.
      y+=dy;
      xmin = (act==8) ? -150 : -64; //set the minimum and maximum x values, depending on the current action (the dog has more area to run than walk)
      xmax = (act==8) ? 300 : 240-64;
      y = constrain(y,ymin, ymax); // constrain the x and y values to their respective mins and maxs
      x = constrain(x,xmin+(y-ymin), xmax-(y-ymin));
      if(++counter>=maxCounts){ // if the incremented frame counter exceeds the maximum number of frames for the current animation
        counter = 0; //reset the frame counter
        if(once){ //if we only are doing the action once (which only happens when sitting)
          once = false; //reset the flag
          (*sitCB)(); // and call the sit callback
        }
      }
      //if we are going to a destination 
      if(goY && ((act==WALK_UP && ydest - y >=0) || (act==WALK_DWN && ydest - y <=0))){ //first check the y axis to see if we're at the destination
        goY = false, yComplete(); //if we are, mark that we're done moving in x, and call the yComplete function.
      } else if(goX && ((act==WALK_LEFT && xdest - x >=0) || (act==WALK_RIGHT && xdest - x <=0)) && gotoFlag){ //otherwise, check if x is complete
        goX = false, gotoFlag = false, (*gotoCB)(); //if it is, mark it as complete, mark the goto cycle as complete, and call the gotoCB
      } 
    } else { //if we aren't allowing the dog to be moved (ie, it's sad), 
      counter = 3; //set the frame pointer to the sad dog frame (it's the fourth frame (zero-indexed) of the SIT_RIGHT animation)
      act = SIT_RIGHT;
    }
    
  }

  //function to change the dog behavior
  void action(Action which, bool override = false){
    act = which; // assign the new behavior
    stepTime = 150; //and set a default frame time
    switch(which){ //set the velocity (dx and dy) and a variety of other flags based on new action
      case WALK_DWN: dx=0, dy=10; break; 
      case WALK_RIGHT: dx=10, dy=0, left = false; break; //left flag controls whether or not animation is flipped.
      case WALK_UP: dx=0, dy=-10; break;
      case WALK_LEFT: dx=-10, dy=0, left=true; break; //left flag controls whether or not animation is flipped.
      case SIT_FOR: dx=0, dy=0; break;
      case STARTSIT: dx=0, dy=0, once = true; break; //once flag set, so this action only happens once.
      case SIT_RIGHT: dx=0, dy=0, maxCounts=2, stepTime = 500; break; //make the steps take longer, and only set max counts to 2, 
      case SLEEP: dx=0, dy=0, maxCounts=2, stepTime = 1000; break; // since there are only two frames for sitting and sleeping.
      case RUN: { //if the dog is now running,
        if(left) dx=-30; //set the x velocity to negative or positive, base on
        else dx = 30;
        //dx = pow(-1,(int)left)*-30; //not used, for readability
        dy=0, maxCounts=3, state=HEART; break; //dogs love to run
      }
    }
    if(act<=5) maxCounts = 4; // if act is any of the walk modes, set num steps to 4
    if(override) bShuffling = false, busy = true; //if we're forcing a specific mode, stop the shuffle, and set busy to true.
  }

  //function to make the dog sit
  void sit(){
    setSitCB([this](){ //set the callback to set the dog action to sitting after startsit finishes. 
      this->action(SIT_RIGHT);
    });
    action(STARTSIT); //make the dog start to sit.
  }

  bool shouldMirror(){
    return left && (act >= 5); //if the dog was/is moving left, and we're no longer in any of the walk modes, return true
  }

  //function for randomizing the walk directions
  void changeWalk(){
    int nw = 0; //init a variable for a new walk direction
    changeTmr = millis()+1000+random(5000); //set a timer for a random time between 1 and 6 seconds from now, to change directions again.
    do { //generate a new random number at least once
      nw = random(4);
    } while (nw == act); //keep generating if the current walk state is equal to the newly generated state.
    action((Action)nw); //make the new walk state happen
  }

  //function to make the dog sad.
  void sad(){
    goTo(120-64,60,[this](){ //when saddened, the dog will go to the middle of the screen
      Serial.println("Got saddened");
      this->freeMove = false; //stop moving freely
      this->act = SIT_RIGHT; //sit facing forward,
      this->bypass = true; // and stop updating the animations
      this->counter = 3;
      notify(10000000); //it will tell you why it's sad for 1000 seconds.
    });
    
  }

  void notify(unsigned long noteTime){
    noteTmr = millis()+noteTime; //set the notification timer for noteTime milliseconds from now
  }

  // function to randomly shuffle through all of the available dog actions.
  void shuffle(){
    int nxt = random(3); //generate a pseudorandom number. On zero, the same action continues.
    int dur = 5000; //and establish a default duration for the new action
    
    //print out some useful info, like whether or not we're running out of ram
    Serial.println("-------Shuffling--------");
    Serial.println(ESP.getFreeHeap());
    Serial.println(act);
    Serial.println(nxt);

    if(act < 4){ //if the dog is walking
      if(nxt == 1) action(RUN); //1 in 3 chance of starting to run
      else if(nxt == 2){ //1 in 3 chance of sitting
        busy = true; //if we decide to sit, mark us as busy until we move to the center of the screen
        dur = 30000; //and set the sit duration to at least 30 seconds
        goTo(120-64, 60, [this](){ // tell the dog to go to the center of the screen
          this->sit(); //once there, sit down
          this->busy = false; //clear the busy flag
          Serial.println("Sat");
        });
      } 
    } else if(act == SIT_RIGHT){ //if we were sitting
      if(nxt == 1) action(SLEEP), dur=60000; //1 in 3 chance of going to sleep for at least 1 minute
      else if(nxt == 2) action(WALK_RIGHT); //1 in 3 chance of starting to walk
    } else if(act == SLEEP){ //if we were sleeping
      if(nxt == 1) action(SIT_RIGHT); //1 in 3 chance of sitting up
      else if(nxt == 2) action(RUN); //1 in 3 chance of starting to do zoomies
    } else if(act == RUN){ //if we were running, always go to the center of the screen and sit down.
      busy = true, goTo(120-64, 60, [this](){
        this->sit();
        this->busy = false;
        Serial.println("Sat");
      });
    }
    int totDur = random(dur) + dur;
    shuffleTmr = millis() + totDur; //shuffle the actions again in dur to dur*2 milliseconds
  }

  //release the dog to do as it pleases (begin shuffling the actions)
  void free(){
    bypass = 0; //clear the bypass flag
    freeMove = true; //let it move around as it pleases
    action(WALK_RIGHT); //as long as it walks to the right first.
    state = HEART; // make it happy
    busy = false; //and not busy
    notify(3000); //let it tell us it's happy for 3 seconds.
    bShuffling = true;
    shuffleTmr = millis() + random(5000)+5000; //and start the shuffling in 5-10 seconds.
  }

  //function called once the y moves of the goto are complete
  void yComplete(){
    Serial.println("At y");
    Action xAct = WALK_RIGHT;
    if(this->xdest < this->x) xAct = WALK_LEFT; //say whether to walk right or left, depending on where the x destination is.
    goX = true;
    this->action(xAct); //start moving in that direction.
  }

  //function for telling the dog where to go.
  template<typename C>
  void goTo(int xDest, int yDest, const C & CB){
    xdest = xDest; //set the x and y destination values.
    ydest = yDest;
    gotoFlag = true; 
    Action yAct = (ydest > y) ? WALK_DWN : WALK_UP; //make the dog walk down if the destination is below the dog, otherwise walk up
    setGotoCB(CB); //set the destination callback.
    goY = true;
    action(yAct);
  }

  //function to feed the dog.
  void giveFood(){
    if(hunger < 100) state = GRUMPY; //it doesn't like being fed unless it's hungry
    else state = HAPPY; //but if it is hungry, it gets happy.
    if(note == FOOD) note = HEART; //if the dog was talking about food, make it talk about being happy.
    hunger = 0; //reset the hunger timer.
    notify(2000); //and tell us about it's emotional state for 2 seconds.
  }

  /////////////////////////////////////////////////////////////
  //this is the function which is called in the main loop.
  /////////////////////////////////////////////////////////////
  void idle(){
    if(stepReady()){ //if we're ready for the next step
      step(); // take the step.

      if(act<4 && freeMove){ //if we're walking around and freely moving
        int yOffset = (y-ymin); // yOffset makes the dogs x range smaller the further down the screen it goes, to make it stay within the circular bounds of the screen.
        while(x +dx + yOffset > xmax || x+dx - yOffset < xmin || y + dy > ymax || y + dy < ymin) changeWalk(); //if the dog has reached the edge of its area, change the walk direction.
        if(changeTmr < millis()){ //or if the random walk direction timer has expired, make it change direction, even if not at an edge.
          changeWalk();
        }
      }
      if(act == RUN){ // if the dog is running
        if(!left && x+dx>xmax-(y-ymin)) x=xmin; // to the right, and it will exceed it's bounded area on the next step, move it to the left edge of the screen
        else if(left && x+dx<xmin+(y-ymin)) x=xmax; // to the left, and it will exceed it's bounded area on the next step, move it to the right edge of the screen
      }

      //if the hungry timer has elapsed
      if(millis()>hngrTmr && !busy){
        hngrTmr = millis() + 180000; //set the timer to expire again in 3 minutes.
        if(hunger < 255) hunger++; // if the dog is not maximally hungry, increment hunger.
        if(hunger == 50) state = FOOD, note=FOOD; // if the dog is a little hungry, make it think about food
        else if(hunger == 150) state = GRUMPY, note=FOOD; //if it's quite hungry, make it grumpy
        else if(hunger == 240) state = MAD, note = FOOD, sad();//, //if it's very hungry, make the dog sad.
      }

      if(millis()>poopTmr && act != SLEEP && !busy){ //if the poop timer has elapsed
        Serial.println(curPoops);
        poopTmr = millis()+10800000; // set the poop timer to expire in three hours
        if(curPoops<maxPoops){ //if we haven't exceeded the maximum number of poops,
          int leftOffset = left?128+16:-16;
          poops[curPoops].move(x+leftOffset,y+112); //move a poop to a position behind the dog
          curPoops++; //and increment the poop counter.
        } else busy = true, state = MAD, sad(), note=POOP; //if we have exceeded the max poops, make the dog sad.
      }

      if(drawCB) (*drawCB)(); // call the rendering callback
    }
     
    if(bShuffling && shuffleTmr < millis()) shuffle(); //if we're shuffling actions, and the timer has expired, shuffle the next action.
  }
};

#endif