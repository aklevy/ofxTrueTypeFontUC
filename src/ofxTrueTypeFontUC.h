#pragma once

#include <vector>
#include "ofRectangle.h"
#include "ofPath.h"
#include "ofImage.h"

//--------------------------------------------------
const static string OF_TTFUC_SANS = "sans-serif";
const static string OF_TTFUC_SERIF = "serif";
const static string OF_TTFUC_MONO = "monospace";

// Line structure for parsing multiline text
struct line {
    std::string text; // text to be displayed in this line
    int top;
    int xHeight;
    int bottom; //extent
    ofRectangle bbox; // bounding box for the line
};

enum ALIGN { LEFT,RIGHT,CENTER };

struct multiline{
    std::vector<line> lines; // all the lines
    int width; // maximum width of lines
    ofRectangle boundingBox; // bounding box for the multiline block
    ALIGN alignType; // align type
};

//--------------------------------------------------
class  ofRectangleUC : public ofRectangle
{
public:
    float top;
};


class ofxTrueTypeFontUC{

public:

    ofxTrueTypeFontUC();
    virtual ~ofxTrueTypeFontUC();
    //  ofxTrueTypeFontUC(const ofxTrueTypeFontUC &);

    //set the default dpi for all typefaces.
    static void setGlobalDpi(int newDpi);


    // 			-- default (without dpi), anti aliased, 96 dpi:
    bool    load            (string filename, int fontsize, bool bAntiAliased=true, bool makeContours=false, float simplifyAmt=0.3, int dpi=0);
    bool    loadFont        (string filename, int fontsize, bool bAntiAliased=true,bool makeContours=false, float simplifyAmt=0.3, int dpi=0);
    void    reloadFont      ();
    void    unloadFont      ();
    void    changeFontSize  (int fontSize);

    // Parsing multiline text
    multiline           parseText       (const std::string txtSrc, int lineWidth, const ofPoint position, ALIGN alignType = LEFT);

    // to draw on the baseLine which position is defined by the argument 'ofPoint position' in parseText
    void    drawMultiline           (const multiline &multi, bool baseLine = false);
    void    drawMultilineAsShapes    (const multiline &multi);

    // get the bounding box of the multiline
    ofRectangle getMultilineBoundingBox(const multiline &multi){return multi.boundingBox;}


    // get the number of words which have been cut during the multiline parsing
    int getNbWordCut(const multiline &multi);

    // new methods using only utf8 and large texture
    void drawString(const std::string &str, float x, float y, bool bindTex = true);

    void bindLargeTex();
    void unbindLargeTex();

    ofRectangleUC getStringBoundingBox(const std::string &str, float x, float y);
    float getLinePositionY(const multiline &multi, int lineNb, bool baseLine);
    float getLastLinePositionY(const multiline &multi, bool baseLine);

    void drawStringAsShapes(const string &str, float x, float y);

    vector<ofPath> getStringAsPoints(const string &str, bool vflip=ofIsVFlipped());

    bool    isLoaded();
    bool    isAntiAliased();

    int     getFontSize();

    float   getLineHeight();
    void    setLineHeight(float height);

    float   getLetterSpacing();
    void    setLetterSpacing(float spacing);

    float   getSpaceSize();
    void    setSpaceSize(float size);

    float   getStringWidth  (const string &str);
    float   getStringHeight (const string &str);
    float   getLetterHeight (const string &str);
    float   getLetterWidth  (const string &str);

    // get the num of loaded chars
    int     getNumCharacters();
    int     getLoadedCharactersCount();
    int     getLimitCharactersNum();
    void    reserveCharacters(int charactersNumber);

    void    setCustomBlending(bool is);

private:
    // disallow copy and assign
    ofxTrueTypeFontUC(const ofxTrueTypeFontUC &);
    void operator=(const ofxTrueTypeFontUC &cpy);
    class Impl;
    Impl *mImpl;


    /* Methods used for multiline */
    // Methods for parsing the text
    std::vector<line>   cutWords                (const std::string wordTxt, int lineWidth, string ltxt,
                                                 int lwidth,  int lheight, ofPoint lpos, int lbot, int ltop);
    line                addNewChar              (line l, int c);
    line                startNewLine            (line l);

    // Methods for drawing multiline
    void                drawStringLine          (const line &sline, int index, bool baseLine = false);
    void                drawLineAsShapes        (const line &sline);

    // Methods to compute the multiline bouding box
    ofRectangle         getLineBoundingBox      (const line &l, int width, ALIGN alignType);
    ofRectangle         getLinesBoundingBox     (const std::vector<line> &lines);

};
