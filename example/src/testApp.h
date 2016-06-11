#pragma once

#include "ofMain.h"
#include "ofxTrueTypeFontUC.h"

class testApp : public ofBaseApp{
  
public:
  void setup();
  void update();
  void draw();
  
  void keyPressed(int key);
  void keyReleased(int key);
  void mouseMoved(int x, int y );
  void mouseDragged(int x, int y, int button);
  void mousePressed(int x, int y, int button);
  void mouseReleased(int x, int y, int button);
  void windowResized(int w, int h);
  void dragEvent(ofDragInfo dragInfo);
  void gotMessage(ofMessage msg);
  
  ofxTrueTypeFontUC myFont;
  multiline         multi;
  ALIGN             alignType;
  bool              drawOnBaseline;
  int               lineWidth;

  string            sampleString;
  ofPoint           p1;
  ofRectangle       rect1;

  bool              displayHelp;
};
