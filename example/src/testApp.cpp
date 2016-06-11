#include "testApp.h"

//--------------------------------------------------------------
void testApp::setup(){
  myFont.loadFont("NotoSansCJKjp-Regular.otf", 15, true, true);
  
  ofBackground(0,0,0);

  /* Multiline example */
  sampleString = "ここでは主にウサギ亜科について記述する（ウサギ目・ウサギ科についてはそれぞれを参照）。\n 現在の分類では、ウサギ亜科には全ての現生ウサギ科を含めるが、かつては一部を含めない分類もあった。\nウサギ目はウサギ科以外に、ナキウサギ科と絶滅したプロラグスなどを含む。";
  alignType = LEFT;
  lineWidth = 400;
  p1 = ofPoint(100, 80);
  drawOnBaseline = false;

  multi = myFont.parseText(sampleString,lineWidth, p1,alignType);

  rect1 = myFont.getMultilineBoundingBox(multi);



  displayHelp = true;
}

//--------------------------------------------------------------
void testApp::update(){
  
}

//--------------------------------------------------------------
void testApp::draw(){
  string fpsStr = "frame rate: " + ofToString(ofGetFrameRate(), 2);
  ofDrawBitmapString(fpsStr, 100, 50);

  ofPushStyle();
  ofSetColor(255,0,0,255);

  // draw line width
  ofDrawBitmapString("line width ", 0, 60);
  ofDrawLine (100, 60, 100 + lineWidth,60);

  // draw bounding box
  ofDrawBitmapString("bbox ", 0, p1.y);
  ofNoFill();
  ofDrawRectangle(rect1);


  ofPopStyle();

  myFont.drawMultiline(multi, drawOnBaseline);
  
  if(displayHelp)
  {
      string helpStr = "Press 'h' to toggle this help message display \n";
      helpStr += "Press '0' or '1' to change font size \n";
      helpStr += "Press '2' or '3' or '4' to change the line width \n";
      helpStr += "Press 'j' to change the text into a japanese one \n";
      helpStr += "Press 'f' to change the text into a french one\n";
      helpStr += "Press 'b' to toggle drawing on the base line corresponding to the upper side of the bbox\n";
      helpStr += "Press 'l'(or 'r', 'c') for left (or right, center) alignment";

      ofPushStyle();
      ofSetColor(255,0,0,255);
     ofDrawBitmapString(helpStr, 100,ofGetWindowHeight()-200);
     ofPopStyle();
  }
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){

    // toggle display help message
    if (key == 'h'){
       displayHelp = !displayHelp;
       return;
    }
    // toggle drawing on the baseline
    else if (key == 'b'){
        drawOnBaseline = !drawOnBaseline;
        return;
    }
    // change font Size
    else if (key == '0'){
        myFont.changeFontSize(8);

    }
    else if (key == '1'){
        myFont.changeFontSize(35);

    }
    // change line width
    else if (key == '2'){
        lineWidth = 100;

    }
    else if (key == '3'){
        lineWidth = 400;
    }
    else if (key == '4'){
        lineWidth = 600;
    }
    // change string to french text
    else if (key == 'f'){
        sampleString = "Lapin est en fait un nom vernaculaire ambigu, désignant une partie seulement des différentes espèces de mammifères classées dans la famille des Léporidés,\n famille qui regroupe à la fois les lièvres et les lapins.";


    }
    // change string to japanese text
    else if (key == 'j'){
        sampleString = "ここでは主にウサギ亜科について記述する（ウサギ目・ウサギ科についてはそれぞれを参照）。\n 現在の分類では、ウサギ亜科には全ての現生ウサギ科を含めるが、かつては一部を含めない分類もあった。\nウサギ目はウサギ科以外に、ナキウサギ科と絶滅したプロラグスなどを含む。";

    }
    // change multilign alignement to left
    else if (key == 'l')
    {
        alignType = ALIGN::LEFT;
    }
    // change multilign alignement to right
    else if (key == 'r')
    {
        alignType = ALIGN::RIGHT;
    }
    // change multilign alignement to center
    else if (key == 'c')
    {
        alignType = ALIGN::CENTER;
    }


    //update multiline structure
    multi = myFont.parseText(sampleString,lineWidth,p1,alignType);
    rect1 = myFont.getMultilineBoundingBox(multi);
}

//--------------------------------------------------------------
void testApp::keyReleased(int key){
  
}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y ){
  
}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){
  
}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){
  
}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){
  
}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h){
  
}

//--------------------------------------------------------------
void testApp::gotMessage(ofMessage msg){
  
}

//--------------------------------------------------------------
void testApp::dragEvent(ofDragInfo dragInfo){
  
}
