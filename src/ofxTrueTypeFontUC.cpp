#include "ofxTrueTypeFontUC.h"
//--------------------------

#include "ft2build.h"

#ifdef TARGET_LINUX
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_TRIGONOMETRY_H
#include <fontconfig/fontconfig.h>
#else
#include "freetype/freetype.h"
#include "freetype/ftglyph.h"
#include "freetype/ftoutln.h"
#include "freetype/fttrigon.h"
#endif

#include <algorithm>

#ifdef TARGET_WIN32
#include <windows.h>
#include <codecvt>
#include <locale>
#endif

#include "ofPoint.h"
#include "ofConstants.h"
#include "ofTexture.h"
#include "ofMesh.h"
#include "ofUtils.h"
#include "utf8.h"
#include "ofGraphics.h"




//===========================================================


//--------------------------------------------------
typedef struct {
    int character;
    int height;
    int width;
    int setWidth;
    int topExtent;
    int leftExtent;
    float tW,tH;
    float x1,x2,y1,y2;
    float t1,t2,v1,v2;
} charPropsUC;


//---------------------------------------------------
class ofxTrueTypeFontUC::Impl {
public:
    Impl() :librariesInitialized_(false)
    {
        m_bHasCustomBlending = false;
    };
    ~Impl() {};


    bool implLoadFont(string filename, int fontsize, bool _bAntiAliased, bool makeContours, float _simplifyAmt, int dpi);
    void implReserveCharacters(int num);
    void implUnloadFont();

    bool bLoadedOk_;
    bool bAntiAliased_;
    int limitCharactersNum_;
    float simplifyAmt_;
    int dpi_;

    vector <ofPath> charOutlines;

    float lineHeight_;
    float letterSpacing_;
    float 	spaceSize_;

    vector<charPropsUC> cps;  // properties for each character
    int	fontSize_;
    bool bMakeContours_;

    void drawChar(int c, float x, float y);
    void drawCharAsShape(int c, float x, float y);

    int	border_;  // visibleBorder;
    string filename_;

    // ofTexture texAtlas;
    vector<ofTexture> textures;
    bool binded_;
    ofMesh stringQuads;

    typedef struct FT_LibraryRec_ * FT_Library;
    typedef struct FT_FaceRec_ * FT_Face;
    FT_Library library_;
    FT_Face face_;
    bool librariesInitialized_;

    bool loadFontFace(string fontname);
    void implChangeFontSize(int fontSize);

    ofPath getCharacterAsPointsFromCharID(const int & charID);

    void bind(const int & charID);
    void unbind(const int & charID);

    int getCharID(const int & c);
    void loadChar(const int & charID);
    vector<int> loadedChars;

    // Image for debug : saving the texture in it
    ofImage img;
    // Large texture containing the letters to be used
    ofTexture largeTex;

    int _xoffset;
    int _yoffset;
    int _heightMax;
    void loadCharInLargeTex(const int & charID);
    void loadData(const void *data, int x, int y, int w, int h, int glFormat, int glType = GL_UNSIGNED_BYTE);
    void bindLargeTex();
    void unbindLargeTex();
    void drawCharLargeTex(int c, float x, float y);

    void setCustomBlending(bool is=true);

    bool m_bHasCustomBlending;

    static const int kTypefaceUnloaded;
    static const int kDefaultLimitCharactersNum;

    void unloadTextures();
    bool initLibraries();
    void finishLibraries();

#ifdef TARGET_OPENGLES
    GLint blend_src, blend_dst_;
    GLboolean blend_enabled_;
    GLboolean texture_2d_enabled_;
#endif
};

static bool printVectorInfo_ = false;
static int ttfGlobalDpi_ = 96;

//--------------------------------------------------------
void ofxTrueTypeFontUC::setGlobalDpi(int newDpi){
    ttfGlobalDpi_ = newDpi;
}

//--------------------------------------------------------
static ofPath makeContoursForCharacter(FT_Face & face);

static ofPath makeContoursForCharacter(FT_Face & face) {

    int nContours	= face->glyph->outline.n_contours;
    int startPos	= 0;

    char * tags		= face->glyph->outline.tags;
    FT_Vector * vec = face->glyph->outline.points;

    ofPath charOutlines;
    charOutlines.setUseShapeColor(false);

    for(int k = 0; k < nContours; k++){
        if( k > 0 ){
            startPos = face->glyph->outline.contours[k-1]+1;
        }
        int endPos = face->glyph->outline.contours[k]+1;

        if(printVectorInfo_){
            ofLogNotice("ofxTrueTypeFontUC") << "--NEW CONTOUR";
        }

        //vector <ofPoint> testOutline;
        ofPoint lastPoint;

        for(int j = startPos; j < endPos; j++){

            if( FT_CURVE_TAG(tags[j]) == FT_CURVE_TAG_ON ){
                lastPoint.set((float)vec[j].x, (float)-vec[j].y, 0);
                if(printVectorInfo_){
                    ofLogNotice("ofxTrueTypeFontUC") << "flag[" << j << "] is set to 1 - regular point - " << lastPoint.x <<  lastPoint.y;
                }
                if (j==startPos){
                    charOutlines.moveTo(lastPoint/64);
                }
                else {
                    charOutlines.lineTo(lastPoint/64);
                }

            }else{
                if(printVectorInfo_){
                    ofLogNotice("ofxTrueTypeFontUC") << "flag[" << j << "] is set to 0 - control point";
                }

                if( FT_CURVE_TAG(tags[j]) == FT_CURVE_TAG_CUBIC ){
                    if(printVectorInfo_){
                        ofLogNotice("ofxTrueTypeFontUC") << "- bit 2 is set to 2 - CUBIC";
                    }

                    int prevPoint = j-1;
                    if( j == 0){
                        prevPoint = endPos-1;
                    }

                    int nextIndex = j+1;
                    if( nextIndex >= endPos){
                        nextIndex = startPos;
                    }

                    ofPoint nextPoint( (float)vec[nextIndex].x,  -(float)vec[nextIndex].y );

                    //we need two control points to draw a cubic bezier
                    bool lastPointCubic =  ( FT_CURVE_TAG(tags[prevPoint]) != FT_CURVE_TAG_ON ) && ( FT_CURVE_TAG(tags[prevPoint]) == FT_CURVE_TAG_CUBIC);

                    if( lastPointCubic ){
                        ofPoint controlPoint1((float)vec[prevPoint].x,	(float)-vec[prevPoint].y);
                        ofPoint controlPoint2((float)vec[j].x, (float)-vec[j].y);
                        ofPoint nextPoint((float) vec[nextIndex].x,	-(float) vec[nextIndex].y);

                        //cubic_bezier(testOutline, lastPoint.x, lastPoint.y, controlPoint1.x, controlPoint1.y, controlPoint2.x, controlPoint2.y, nextPoint.x, nextPoint.y, 8);
                        charOutlines.bezierTo(controlPoint1.x/64, controlPoint1.y/64, controlPoint2.x/64, controlPoint2.y/64, nextPoint.x/64, nextPoint.y/64);
                    }

                }else{

                    ofPoint conicPoint( (float)vec[j].x,  -(float)vec[j].y );

                    if(printVectorInfo_){
                        ofLogNotice("ofxTrueTypeFontUC") << "- bit 2 is set to 0 - conic- ";
                        ofLogNotice("ofxTrueTypeFontUC") << "--- conicPoint point is " << conicPoint.x << conicPoint.y;
                    }

                    //If the first point is connic and the last point is connic then we need to create a virutal point which acts as a wrap around
                    if( j == startPos ){
                        bool prevIsConnic = (  FT_CURVE_TAG( tags[endPos-1] ) != FT_CURVE_TAG_ON ) && ( FT_CURVE_TAG( tags[endPos-1]) != FT_CURVE_TAG_CUBIC );

                        if( prevIsConnic ){
                            ofPoint lastConnic((float)vec[endPos - 1].x, (float)-vec[endPos - 1].y);
                            lastPoint = (conicPoint + lastConnic) / 2;

                            if(printVectorInfo_){
                                ofLogNotice("ofxTrueTypeFontUC") << "NEED TO MIX WITH LAST";
                                ofLogNotice("ofxTrueTypeFontUC") << "last is " << lastPoint.x << " " << lastPoint.y;
                            }
                        }
                        else {
                            lastPoint.set((float)vec[endPos-1].x, (float)-vec[endPos-1].y, 0);
                        }
                    }

                    //bool doubleConic = false;

                    int nextIndex = j+1;
                    if( nextIndex >= endPos){
                        nextIndex = startPos;
                    }

                    ofPoint nextPoint( (float)vec[nextIndex].x,  -(float)vec[nextIndex].y );

                    if(printVectorInfo_){
                        ofLogNotice("ofxTrueTypeFontUC") << "--- last point is " << lastPoint.x << " " <<  lastPoint.y;
                    }

                    bool nextIsConnic = (  FT_CURVE_TAG( tags[nextIndex] ) != FT_CURVE_TAG_ON ) && ( FT_CURVE_TAG( tags[nextIndex]) != FT_CURVE_TAG_CUBIC );

                    //create a 'virtual on point' if we have two connic points
                    if( nextIsConnic ){
                        nextPoint = (conicPoint + nextPoint) / 2;
                        if(printVectorInfo_){
                            ofLogNotice("ofxTrueTypeFontUC") << "|_______ double connic!";
                        }
                    }
                    if(printVectorInfo_){
                        ofLogNotice("ofxTrueTypeFontUC") << "--- next point is " << nextPoint.x << " " << nextPoint.y;
                    }

                    //quad_bezier(testOutline, lastPoint.x, lastPoint.y, conicPoint.x, conicPoint.y, nextPoint.x, nextPoint.y, 8);
                    charOutlines.quadBezierTo(lastPoint.x/64, lastPoint.y/64, conicPoint.x/64, conicPoint.y/64, nextPoint.x/64, nextPoint.y/64);

                    if( nextIsConnic ){
                        lastPoint = nextPoint;
                    }
                }
            }

            //end for
        }
        charOutlines.close();
    }

    return charOutlines;
}


#if defined(TARGET_ANDROID) || defined(TARGET_OF_IOS)
#include <set>

static set<ofxTrueTypeFontUC*> & all_fonts(){
    static set<ofxTrueTypeFontUC*> *all_fonts = new set<ofxTrueTypeFontUC*>;
    return *all_fonts;
}

void ofUnloadAllFontTextures(){
    set<ofxTrueTypeFontUC*>::iterator it;
    for (it=all_fonts().begin(); it!=all_fonts().end(); ++it) {
        (*it)->unloadFont();
    }
}

void ofReloadAllFontTextures(){
    set<ofxTrueTypeFontUC*>::iterator it;
    for (it=all_fonts().begin(); it!=all_fonts().end(); ++it) {
        (*it)->reloadFont();
    }
}
#endif


#ifdef TARGET_OSX
static string osxFontPathByName( string fontname ){
    CFStringRef targetName = CFStringCreateWithCString(NULL, fontname.c_str(), kCFStringEncodingUTF8);
    CTFontDescriptorRef targetDescriptor = CTFontDescriptorCreateWithNameAndSize(targetName, 0.0);
    CFURLRef targetURL = (CFURLRef) CTFontDescriptorCopyAttribute(targetDescriptor, kCTFontURLAttribute);
    string fontPath = "";

    if (targetURL) {
        UInt8 buffer[PATH_MAX];
        CFURLGetFileSystemRepresentation(targetURL, true, buffer, PATH_MAX);
        fontPath = string((char *)buffer);
        CFRelease(targetURL);
    }

    CFRelease(targetName);
    CFRelease(targetDescriptor);

    return fontPath;
}
#endif

#ifdef TARGET_WIN32
#include <map>
// font font face -> file name name mapping
static map<string, string> fonts_table;
// read font linking information from registry, and store in std::map
static void initWindows(){
    LONG l_ret;

    const wchar_t *Fonts = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";

    HKEY key_ft;
    l_ret = RegOpenKeyExW(HKEY_LOCAL_MACHINE, Fonts, 0, KEY_QUERY_VALUE, &key_ft);
    if (l_ret != ERROR_SUCCESS) {
        ofLogError("ofxTrueTypeFontUC") << "initWindows(): couldn't find fonts registery key";
        return;
    }

    DWORD value_count;
    DWORD max_data_len;
    wchar_t value_name[2048];
    BYTE *value_data;


    // get font_file_name -> font_face mapping from the "Fonts" registry key

    l_ret = RegQueryInfoKeyW(key_ft, NULL, NULL, NULL, NULL, NULL, NULL, &value_count, NULL, &max_data_len, NULL, NULL);
    if (l_ret != ERROR_SUCCESS) {
        ofLogError("ofxTrueTypeFontUC") << "initWindows(): couldn't query registery for fonts";
        return;
    }

    // no font installed
    if (value_count == 0) {
        ofLogError("ofxTrueTypeFontUC") << "initWindows(): couldn't find any fonts in registery";
        return;
    }

    // max_data_len is in BYTE
    value_data = static_cast<BYTE *>(HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, max_data_len));
    if (value_data == NULL) return;

    char value_name_char[2048];
    char value_data_char[2048];
    /*char ppidl[2048];
    char fontsPath[2048];
    SHGetKnownFolderIDList(FOLDERID_Fonts, 0, NULL, &ppidl);
    SHGetPathFromIDList(ppidl,&fontsPath);*/
    string fontsDir = getenv ("windir");
    fontsDir += "\\Fonts\\";
    for (DWORD i = 0; i < value_count; ++i) {
        DWORD name_len = 2048;
        DWORD data_len = max_data_len;

        l_ret = RegEnumValueW(key_ft, i, value_name, &name_len, NULL, NULL, value_data, &data_len);
        if (l_ret != ERROR_SUCCESS) {
            ofLogError("ofxTrueTypeFontUC") << "initWindows(): couldn't read registry key for font type";
            continue;
        }

        wcstombs(value_name_char,value_name,2048);
        wcstombs(value_data_char,reinterpret_cast<wchar_t *>(value_data),2048);
        string curr_face = value_name_char;
        string font_file = value_data_char;
        curr_face = curr_face.substr(0, curr_face.find('(') - 1);
        fonts_table[curr_face] = fontsDir + font_file;
    }

    HeapFree(GetProcessHeap(), 0, value_data);

    l_ret = RegCloseKey(key_ft);
}

static string winFontPathByName( string fontname ){
    if (fonts_table.find(fontname)!=fonts_table.end()) {
        return fonts_table[fontname];
    }
    for (map<string,string>::iterator it = fonts_table.begin(); it!=fonts_table.end(); it++) {
        if (ofIsStringInString(ofToLower(it->first),ofToLower(fontname)))
            return it->second;
    }
    return "";
}
#endif

#ifdef TARGET_LINUX
static string linuxFontPathByName(string fontname){
    string filename;
    FcPattern * pattern = FcNameParse((const FcChar8*)fontname.c_str());
    FcBool ret = FcConfigSubstitute(0,pattern,FcMatchPattern);
    if (!ret) {
        ofLogError() << "linuxFontPathByName(): couldn't find font file or system font with name \"" << fontname << "\"";
        return "";
    }
    FcDefaultSubstitute(pattern);
    FcResult result;
    FcPattern * fontMatch=NULL;
    fontMatch = FcFontMatch(0,pattern,&result);

    if (!fontMatch) {
        ofLogError() << "linuxFontPathByName(): couldn't match font file or system font with name \"" << fontname << "\"";
        return "";
    }
    FcChar8 *file;
    if (FcPatternGetString (fontMatch, FC_FILE, 0, &file) == FcResultMatch) {
        filename = (const char*)file;
    }
    else {
        ofLogError() << "linuxFontPathByName(): couldn't find font match for \"" << fontname << "\"";
        return "";
    }
    return filename;
}
#endif

//------------------------------------------------------------------
ofxTrueTypeFontUC::ofxTrueTypeFontUC() {
    mImpl = new Impl();
    mImpl->bLoadedOk_	= false;
    mImpl->bMakeContours_	= false;
#if defined(TARGET_ANDROID) || defined(TARGET_OF_IOS)
    all_fonts().insert(this);
#endif
    mImpl->letterSpacing_ = 1;
    mImpl->spaceSize_ = 1;

    // 3 pixel border around the glyph
    // We show 2 pixels of this, so that blending looks good.
    // 1 pixels is hidden because we don't want to see the real edge of the texture
    mImpl->border_ = 3;

    mImpl->stringQuads.setMode(OF_PRIMITIVE_TRIANGLES);
    mImpl->binded_ = false;

    mImpl->limitCharactersNum_ = mImpl->kDefaultLimitCharactersNum;
}

//------------------------------------------------------------------
ofxTrueTypeFontUC::~ofxTrueTypeFontUC() {

    if (mImpl->bLoadedOk_)
        unloadFont();

#if defined(TARGET_ANDROID) || defined(TARGET_OF_IOS)
    all_fonts().erase(this);
#endif

    if (mImpl != NULL)
        delete mImpl;
}

void ofxTrueTypeFontUC::Impl::implUnloadFont() {
    if(!bLoadedOk_)
        return;


    textures.clear();

    cps.clear();
    largeTex.clear();

    loadedChars.clear();

    // ------------- close the library and typeface
    FT_Done_Face(face_);
    FT_Done_FreeType(library_);

    bLoadedOk_ = false;
}

void ofxTrueTypeFontUC::unloadFont() {
    mImpl->implUnloadFont();
}
void ofxTrueTypeFontUC::Impl::implChangeFontSize(int fontSize) {
    fontSize_ = fontSize;
    FT_Set_Char_Size(face_,fontSize_ <<6, fontSize_ << 6, dpi_, dpi_);
}
void ofxTrueTypeFontUC::changeFontSize(int fontSize) {
    mImpl->implChangeFontSize(fontSize);
    reloadFont();
}

bool ofxTrueTypeFontUC::Impl::loadFontFace(string fontname){
    filename_ = ofToDataPath(fontname,true);
    ofFile fontFile(filename_, ofFile::Reference);
    int fontID = 0;
    if (!fontFile.exists()) {
#ifdef TARGET_LINUX
        filename_ = linuxFontPathByName(fontname);
#elif defined(TARGET_OSX)
        if (fontname==OF_TTFUC_SANS) {
            fontname = "Helvetica Neue";
            fontID = 4;
        }
        else if (fontname==OF_TTFUC_SERIF) {
            fontname = "Times New Roman";
        }
        else if (fontname==OF_TTFUC_MONO) {
            fontname = "Menlo Regular";
        }
        filename_ = osxFontPathByName(fontname);
#elif defined(TARGET_WIN32)
        if (fontname==OF_TTFUC_SANS) {
            fontname = "Arial";
        }
        else if (fontname==OF_TTFUC_SERIF) {
            fontname = "Times New Roman";
        }
        else if (fontname==OF_TTFUC_MONO) {
            fontname = "Courier New";
        }
        filename_ = winFontPathByName(fontname);
#endif
        if (filename_ == "" ) {
            ofLogError("ofxTrueTypeFontUC") << "loadFontFace(): couldn't find font \"" << fontname << "\"";
            return false;
        }
        ofLogVerbose("ofxTrueTypeFontUC") << "loadFontFace(): \"" << fontname << "\" not a file in data loading system font from \"" << filename_ << "\"";
    }
    FT_Error err;
    err = FT_New_Face( library_, filename_.c_str(), fontID, &face_ );
    if (err) {
        // simple error table in lieu of full table (see fterrors.h)
        string errorString = "unknown freetype";
        if(err == 1) errorString = "INVALID FILENAME";
        ofLogError("ofxTrueTypeFontUC") << "loadFontFace(): couldn't create new face for \"" << fontname << "\": FT_Error " << err << " " << errorString;
        return false;
    }

    return true;
}

void ofxTrueTypeFontUC::reloadFont() {
    mImpl->implLoadFont(mImpl->filename_, mImpl->fontSize_, mImpl->bAntiAliased_, mImpl->bMakeContours_, mImpl->simplifyAmt_, mImpl->dpi_);
}

//-----------------------------------------------------------
bool ofxTrueTypeFontUC::loadFont(string filename, int fontsize, bool bAntiAliased, bool makeContours, float simplifyAmt, int dpi) {
    return mImpl->implLoadFont(filename, fontsize, bAntiAliased, makeContours, simplifyAmt, dpi);

}

bool ofxTrueTypeFontUC::Impl::implLoadFont(string filename, int fontsize, bool bAntiAliased, bool makeContours, float simplifyAmt, int dpi) {
    bMakeContours_ = makeContours;

    //------------------------------------------------
    if (bLoadedOk_ == true){
        // we've already been loaded, try to clean up :
        implUnloadFont();
    }
    //------------------------------------------------

    dpi_ = dpi;
    if( dpi_ == 0 ){
        dpi_ = ttfGlobalDpi_;
    }

    filename_ = ofToDataPath(filename);

    bLoadedOk_ = false;
    bAntiAliased_ = bAntiAliased;
    fontSize_ = fontsize;
    simplifyAmt_ = simplifyAmt;

    _xoffset = 10;
    _yoffset = 10;
    _heightMax = 0;

    //--------------- load the library and typeface
    FT_Error err = FT_Init_FreeType(&library_);
    if (err) {
        ofLog(OF_LOG_ERROR,"ofxTrueTypeFontUC::loadFont - Error initializing freetype lib: FT_Error = %d", err);
        return false;
    }
    err = FT_New_Face(library_, filename_.c_str(), 0, &face_);

    if (err) {
        // simple error table in lieu of full table (see fterrors.h)
        string errorString = "unknown freetype";
        if (err == 1)
            errorString = "INVALID FILENAME";
        ofLog(OF_LOG_ERROR,"ofxTrueTypeFontUC::loadFont - %s: %s: FT_Error = %d", errorString.c_str(), filename_.c_str(), err);
        return false;
    }

    FT_Set_Char_Size(face_, fontSize_ << 6, fontSize_ << 6, dpi_, dpi_);
    lineHeight_ = fontSize_ * 1.43f;

    //------------------------------------------------------
    //kerning would be great to support:
    //ofLog(OF_LOG_NOTICE,"FT_HAS_KERNING ? %i", FT_HAS_KERNING(face));
    //------------------------------------------------------

    implReserveCharacters(limitCharactersNum_);

    bLoadedOk_ = true;
    return true;
}

//-----------------------------------------------------------
bool ofxTrueTypeFontUC::isLoaded() {
    return mImpl->bLoadedOk_;
}

//-----------------------------------------------------------
bool ofxTrueTypeFontUC::isAntiAliased() {
    return mImpl->bAntiAliased_;
}

//-----------------------------------------------------------
int ofxTrueTypeFontUC::getFontSize() {
    return mImpl->fontSize_;
}

//-----------------------------------------------------------
void ofxTrueTypeFontUC::setLineHeight(float _newLineHeight) {
    mImpl->lineHeight_ = _newLineHeight;
}

//-----------------------------------------------------------
float ofxTrueTypeFontUC::getLineHeight() {
    return mImpl->lineHeight_;
}

//-----------------------------------------------------------
void ofxTrueTypeFontUC::setLetterSpacing(float _newletterSpacing) {
    mImpl->letterSpacing_ = _newletterSpacing;
}

//-----------------------------------------------------------
float ofxTrueTypeFontUC::getLetterSpacing() {
    return mImpl->letterSpacing_;
}

//-----------------------------------------------------------
void ofxTrueTypeFontUC::setSpaceSize(float _newspaceSize) {
    mImpl->spaceSize_ = _newspaceSize;
}

//-----------------------------------------------------------
float ofxTrueTypeFontUC::getSpaceSize() {
    return mImpl->spaceSize_;
}

ofPath ofxTrueTypeFontUC::Impl::getCharacterAsPointsFromCharID(const int &charID) {
    if (bMakeContours_ == false)
        ofLog(OF_LOG_ERROR, "getCharacterAsPoints: contours not created,  call loadFont with makeContours set to true");

    if (bMakeContours_ && (int)charOutlines.size() > 0)
        return charOutlines[charID];
    else {
        if(charOutlines.empty())
            charOutlines.push_back(ofPath());
        return charOutlines[0];
    }
}

//-----------------------------------------------------------
void ofxTrueTypeFontUC::Impl::drawChar(int c, float x, float y) {

    if (c >= limitCharactersNum_) {
        //ofLog(OF_LOG_ERROR,"Error : char (%i) not allocated -- line %d in %s", (c + NUM_CHARACTER_TO_START), __LINE__,__FILE__);
        return;
    }
    GLfloat	x1, y1, x2, y2;
    GLfloat t1, v1, t2, v2;
    t2 = cps[c].t2;
    v2 = cps[c].v2;
    t1 = cps[c].t1;
    v1 = cps[c].v1;

    x1 = cps[c].x1+x;
    y1 = cps[c].y1+y;
    x2 = cps[c].x2+x;
    y2 = cps[c].y2+y;

    int firstIndex = stringQuads.getVertices().size();

    stringQuads.addVertex(ofVec3f(x1,y1));
    stringQuads.addVertex(ofVec3f(x2,y1));
    stringQuads.addVertex(ofVec3f(x2,y2));
    stringQuads.addVertex(ofVec3f(x1,y2));

    stringQuads.addTexCoord(ofVec2f(t1,v1));
    stringQuads.addTexCoord(ofVec2f(t2,v1));
    stringQuads.addTexCoord(ofVec2f(t2,v2));
    stringQuads.addTexCoord(ofVec2f(t1,v2));

    stringQuads.addIndex(firstIndex);
    stringQuads.addIndex(firstIndex+1);
    stringQuads.addIndex(firstIndex+2);
    stringQuads.addIndex(firstIndex+2);
    stringQuads.addIndex(firstIndex+3);
    stringQuads.addIndex(firstIndex);
}
//-----------------------------------------------------------
void ofxTrueTypeFontUC::Impl::drawCharLargeTex(int c, float x, float y) {

    if (c >= limitCharactersNum_) {
        //ofLog(OF_LOG_ERROR,"Error : char (%i) not allocated -- line %d in %s", (c + NUM_CHARACTER_TO_START), __LINE__,__FILE__);
        return;
    }
    if(binded_)
        stringQuads.clear();

    GLfloat	x1, y1, x2, y2;
    GLfloat t1, v1, t2, v2;
    t2 = cps[c].t2;
    v2 = cps[c].v2;
    t1 = cps[c].t1;
    v1 = cps[c].v1;

    x1 = cps[c].x1+x;
    y1 = cps[c].y1+y;
    x2 = cps[c].x2+x;
    y2 = cps[c].y2+y;

    int firstIndex = stringQuads.getVertices().size();

    stringQuads.addVertex(ofVec3f(x1,y1));
    stringQuads.addVertex(ofVec3f(x2,y1));
    stringQuads.addVertex(ofVec3f(x2,y2));
    stringQuads.addVertex(ofVec3f(x1,y2));

    stringQuads.addTexCoord(ofVec2f(t1,v1));
    stringQuads.addTexCoord(ofVec2f(t2,v1));
    stringQuads.addTexCoord(ofVec2f(t2,v2));
    stringQuads.addTexCoord(ofVec2f(t1,v2));

    stringQuads.addIndex(firstIndex);
    stringQuads.addIndex(firstIndex+1);
    stringQuads.addIndex(firstIndex+2);
    stringQuads.addIndex(firstIndex+2);
    stringQuads.addIndex(firstIndex+3);
    stringQuads.addIndex(firstIndex);

    if (binded_) {
        stringQuads.drawFaces();
    }
}

//-----------------------------------------------------------
vector<ofPath> ofxTrueTypeFontUC::getStringAsPoints(const string &src, bool vflip){
    vector<ofPath> shapes;

    if (!mImpl->bLoadedOk_) {
        ofLog(OF_LOG_ERROR,"Error : font not allocated -- line %d in %s", __LINE__,__FILE__);
        return shapes;
    }

    GLint index	= 0;
    GLfloat X = 0;
    GLfloat Y = 0;
    int newLineDirection = 1;
    int c      = 0;
    int cy     = 0;

    if (!vflip) {
        // this would align multiline texts to the last line when vflip is disabled
        //int lines = ofStringTimesInString(c,"\n");
        //Y = lines*lineHeight;
        newLineDirection = -1;
    }
    std::string::const_iterator b = src.begin();
    std::string::const_iterator e = src.end();

    try{
        while(b != e){

            c =  utf8::next(b,e);

            if (c == '\n') {
                Y += mImpl->lineHeight_ * newLineDirection;
                X = 0;
            }
            else if (c == ' ') {
                cy = mImpl->getCharID('p');
                X += mImpl->cps[cy].setWidth * mImpl->letterSpacing_ * mImpl->spaceSize_;
            }
            else {
                cy = mImpl->getCharID(c);
                if (mImpl->cps[cy].character == mImpl->kTypefaceUnloaded)
                    mImpl->loadChar(cy);
                shapes.push_back(mImpl->getCharacterAsPointsFromCharID(cy));
                shapes.back().translate(ofPoint(X,Y));
                X += mImpl->cps[cy].setWidth * mImpl->letterSpacing_;
            }
            index++;
        }
    }
    catch( utf8::invalid_utf8& e)
    {
        std::cerr<< "CAUGHT EXCEPTION : " << e.what() << std::endl;
    }
    catch( utf8::not_enough_room& e)
    {
        std::cerr<< "CAUGHT EXCEPTION : " << e.what() << std::endl;
    }
    return shapes;
}

//-----------------------------------------------------------
void ofxTrueTypeFontUC::Impl::drawCharAsShape(int c, float x, float y) {
    if (c >= limitCharactersNum_) {
        //ofLog(OF_LOG_ERROR,"Error : char (%i) not allocated -- line %d in %s", (c + NUM_CHARACTER_TO_START), __LINE__,__FILE__);
        return;
    }
    //-----------------------

    int cu = c;
    ofPath & charRef = charOutlines[cu];
    charRef.setFilled(ofGetStyle().bFill);
    charRef.draw(x,y);
}


float ofxTrueTypeFontUC::getStringWidth(const string &str) {
    ofRectangle rect = getStringBoundingBox(str, 0,0);
    return rect.width;
}

float ofxTrueTypeFontUC::getStringHeight(const string &str) {
    ofRectangle rect = getStringBoundingBox(str, 0,0);
    return rect.height;
}

//=====================================================================
float ofxTrueTypeFontUC::getLetterHeight(const string &str){

    std::string::const_iterator b = str.begin();
    std::string::const_iterator e = str.end();

    try{

        int c =  utf8::next(b,e);
        int cy = mImpl->getCharID(c);

        if (mImpl->cps[cy].character == mImpl->kTypefaceUnloaded){
            mImpl->loadCharInLargeTex(cy);
        }

        return mImpl->cps[cy].height;

    }
    catch( utf8::invalid_utf8& e)
    {
        std::cerr<< "CAUGHT EXCEPTION : " << e.what() << std::endl;
    }
    catch( utf8::not_enough_room& e)
    {
        std::cerr<< "CAUGHT EXCEPTION : " << e.what() << std::endl;
    }

    return 0.;
}

//=====================================================================
float ofxTrueTypeFontUC::getLetterWidth(const string &str){

    std::string::const_iterator b = str.begin();
    std::string::const_iterator e = str.end();

    try{

        int c =  utf8::next(b,e);
        int cy = mImpl->getCharID(c);

        if (mImpl->cps[cy].character == mImpl->kTypefaceUnloaded){
            mImpl->loadCharInLargeTex(cy);
        }

        return mImpl->cps[cy].width;

    }
    catch( utf8::invalid_utf8& e)
    {
        std::cerr<< "CAUGHT EXCEPTION : " << e.what() << std::endl;
    }
    catch( utf8::not_enough_room& e)
    {
        std::cerr<< "CAUGHT EXCEPTION : " << e.what() << std::endl;
    }

    return 0.;
}


//=====================================================================
// Parsing  text
//--------------------------------------------------
std::vector<line> ofxTrueTypeFontUC::cutWords(const std::string wordTxt, int lineWidth,std::string ltxt, int lwidth, int lheight, ofPoint lpos, int lbot, int ltop){

    std::vector<line> vecLinesWord;

    line l;
    l.text          = ltxt;
    l.bbox.width    = lwidth;
    l.bbox.height   = lheight;
    l.bbox.position = lpos;
    l.bottom        = lbot;
    l.top           = ltop;
    l.xHeight = mImpl->cps[mImpl->getCharID('x')].tH;

    std::string::const_iterator b = wordTxt.begin();
    std::string::const_iterator e = wordTxt.end();
    const int wordLength = utf8::distance(b,e);
    int count = 1;

    // load hyphen '-'
    int hyphenid = mImpl->getCharID('-');
    if (mImpl->cps[hyphenid].character == mImpl->kTypefaceUnloaded)
        mImpl->loadCharInLargeTex(hyphenid);
    int hyphenLength = mImpl->cps[mImpl->getCharID('-')].setWidth;

    bool cut    = false;
    bool last   = false;

    int c,cnext;
    c = cnext = 0;
    int cy,cnexty;
    cy = cnexty = 0;

    int btmp    = 0;
    int toptmp  = 0;
    try{
        while( count < wordLength) {
            c =  utf8::next (b, e);
            cy = mImpl->getCharID(c);

            cnext   = utf8::peek_next (b, e);
            cnexty  = mImpl->getCharID(cnext);

            // if only two letters are left
            if ( count == (wordLength - 1) ) {

                // if both characters get in the line, end of the word
                if ( l.bbox.width
                     + mImpl->cps[cy].setWidth * mImpl->letterSpacing_
                     + mImpl->cps[cnexty].setWidth < lineWidth ){

                    // add both letters
                    utf8::append(c,back_inserter(l.text));
                    l.bbox.setWidth (l.bbox.getWidth() +  mImpl->cps[cy].setWidth);

                    // if the letter has pixels under the base line
                    btmp = mImpl->cps[cy].topExtent - mImpl->cps[cy].height;
                    if (btmp > l.bottom){
                        l.bbox.setHeight(l.bbox.getHeight() + (btmp - l.bottom));
                        l.bottom = btmp;
                    }
                    else{
                        l.bbox.setHeight(max(mImpl->cps[cy].tH ,l.bbox.getHeight()));
                    }
                    // if the letter is bigger than 'x' (top)
                    toptmp = mImpl->cps[cy].height - l.xHeight;

                    if (toptmp > l.top){
                        l.bbox.setHeight(l.bbox.getHeight() + (toptmp - l.top));
                        l.top = toptmp;
                    }

                    // add last letter
                    utf8::append(cnext,back_inserter(l.text));
                    l.bbox.setWidth (l.bbox.getWidth() +  mImpl->cps[cnexty].setWidth);

                    btmp = mImpl->cps[cnexty].topExtent - mImpl->cps[cnexty].height;
                    if (btmp > l.bottom){
                        l.bbox.setHeight(l.bbox.getHeight() + (btmp - l.bottom));
                        l.bottom = btmp;
                    }
                    else{
                        l.bbox.setHeight(max(mImpl->cps[cnexty].tH ,l.bbox.getHeight()));
                    }

                    // if the letter is bigger than 'x' (top)
                    toptmp = mImpl->cps[cnexty].height - l.xHeight;

                    if (toptmp > l.top){
                        l.bbox.setHeight(l.bbox.getHeight() + (toptmp - l.top));
                        l.top = toptmp;
                    }
                    // add to the line vector
                    vecLinesWord.push_back(l);

                }else{ // cut the word

                    cut = true;
                    last = true;
                }
                // else same behaviour as other characters
            }
            // more than two characters left
            else if ( count < (wordLength - 1)  ){

                // if both (current and next) characters and hyphen get in the line
                if ( l.bbox.getWidth()
                     + mImpl->cps[cy].setWidth * mImpl->letterSpacing_
                     + mImpl->cps[cnexty].setWidth
                     + hyphenLength < lineWidth ){

                    // add current character
                    utf8::append(c,back_inserter(l.text));
                    l.bbox.setWidth     (l.bbox.getWidth() +  mImpl->cps[cy].setWidth);

                    btmp = mImpl->cps[cy].topExtent - mImpl->cps[cy].height;
                    if (btmp > l.bottom){
                        l.bbox.setHeight(l.bbox.getHeight() + (btmp - l.bottom));
                        l.bottom = btmp;
                    }
                    else{
                        l.bbox.setHeight(max(mImpl->cps[cy].tH ,l.bbox.getHeight()));
                    }
                    // if the letter is bigger than 'x' (top)
                    toptmp = mImpl->cps[cy].height - l.xHeight;

                    if (toptmp > l.top){
                        l.bbox.setHeight(l.bbox.getHeight() + (toptmp - l.top));
                        l.top = toptmp;
                    }
                }

                else if ( l.bbox.getWidth()
                          + mImpl->cps[cy].setWidth * mImpl->letterSpacing_
                          + hyphenLength < lineWidth ){

                    cut = true;
                }
                else{

                }
            }

            if (cut) {
                // add current character to the current line
                utf8::append(c,back_inserter(l.text));
                l.bbox.setWidth(l.bbox.getWidth() + mImpl->cps[cy].setWidth * mImpl->letterSpacing_);

                // if the letter has pixels under the base line
                btmp = mImpl->cps[cy].topExtent - mImpl->cps[cy].height;
                if (btmp > l.bottom){
                    l.bbox.setHeight(l.bbox.getHeight() + (btmp - l.bottom));
                    l.bottom = btmp;
                }
                else{
                    l.bbox.setHeight(max(mImpl->cps[cy].tH ,l.bbox.getHeight()));
                }
                // if the letter is bigger than 'x' (top)
                toptmp = mImpl->cps[cy].height - l.xHeight;

                if (toptmp > l.top){
                    l.bbox.setHeight(l.bbox.getHeight() + (toptmp - l.top));
                    l.top = toptmp;
                }

                // add hyphen to the current line
                l.text.append("-");
                l.bbox.setWidth( l.bbox.getWidth() + hyphenLength);

                // add this current line to the vector
                vecLinesWord.push_back(l);

                // start a new line
                l = startNewLine(l);

                if(last){
                    utf8::append(cnext,back_inserter(l.text));
                    l.bbox.setWidth     ( l.bbox.getWidth() + mImpl->cps[cnexty].setWidth);
                    btmp = mImpl->cps[cy].topExtent - mImpl->cps[cnexty].height;
                    if (btmp > l.bottom){
                        l.bbox.setHeight(l.bbox.getHeight() + (btmp - l.bottom));
                        l.bottom = btmp;
                    }
                    else{
                        l.bbox.setHeight(max(mImpl->cps[cnexty].tH ,l.bbox.getHeight()));
                    }
                    // if the letter is bigger than 'x' (top)
                    toptmp = mImpl->cps[cy].height - l.xHeight;
                    if (toptmp > l.top){
                        l.bbox.setHeight(l.bbox.getHeight() + (toptmp - l.top));
                        l.top = toptmp;
                    }

                    // add this current line to the vector
                    vecLinesWord.push_back(l);

                    last = false;
                }
                cut = false;
            }
            count ++;
        }
    }
    catch( utf8::invalid_utf8& e)
    {
        ofLog() << "CAUGHT EXCEPTION : " << e.what();
    }
    catch( utf8::not_enough_room& e)
    {
        ofLog() << "CAUGHT EXCEPTION : " << e.what();
    }

    return vecLinesWord;
}


line ofxTrueTypeFontUC::startNewLine(line l){
    line newLine;
    newLine.text = "";
    newLine.bbox = ofRectangle(l.bbox.position + ofPoint(0, mImpl->lineHeight_ + l.top), 0, l.xHeight);
    newLine.top = 0;
    newLine.xHeight = l.xHeight;
    newLine.bottom = 0;
    return newLine;
}

line ofxTrueTypeFontUC::addNewChar(line l, int c){
    line newLine = l;
    int cy = mImpl->getCharID(c);

    utf8::append(c,back_inserter(newLine.text));

    // adds the letter width to the line width
    newLine.bbox.setWidth     (l.bbox.getWidth() + mImpl->cps[cy].setWidth * mImpl->letterSpacing_);
    newLine.bbox.setHeight    (max(mImpl->cps[cy].tH,l.bbox.getHeight()));
    newLine.bottom = max((int) (mImpl->cps[cy].tH- mImpl->cps[cy].height) , l.bottom);

    return newLine;
}

multiline ofxTrueTypeFontUC::parseText(const std::string txtSrc, int lineWidth, const ofPoint position, ALIGN alignType){
    multiline multi;
    multi.width         = 0;
    multi.boundingBox   = ofRectangle(position,0,0);
    multi.alignType     = alignType;

    std::vector<line> vecLines;
    line l;

    l.text = "";
    l.bbox = ofRectangle(position,0,0);
    l.bottom = 0;
    l.top = 0;
    l.xHeight = 0;

    std::string wordtmp = "";
    int wordWidthtmp    = 0;
    int wordHeighttmp   = 0;
    int wordToptmp      = 0;
    int wordBottomtmp   = 0;

    std::string::const_iterator b = txtSrc.begin();
    std::string::const_iterator e = txtSrc.end();

    int c, cprev, cy;
    c = cprev = cy = 0;

    // load space ' '
    int pid = mImpl->getCharID('p');
    if (mImpl->cps[pid].character == mImpl->kTypefaceUnloaded)
    {
        mImpl->loadCharInLargeTex(pid);
        setSpaceSize(mImpl->cps[pid].width);
    }

    // load 'x' to have the xHeight for the line structure
    int xid = mImpl->getCharID('x');
    if (mImpl->cps[xid].character == mImpl->kTypefaceUnloaded)
        mImpl->loadCharInLargeTex(xid);


    try{
        while(b != e){

            cprev = c;
            c     =  utf8::next(b,e);

            /*if (c == 0xA0)
        {
        //ofLog() << "found insec";
        }*/

            // if the character was not loaded before, load here :
            cy = mImpl->getCharID(c);

            if (c!=' ' && c!=0xA0 && mImpl->cps[cy].character == mImpl->kTypefaceUnloaded){
                mImpl->loadCharInLargeTex(cy);
            }

            // end of latin word
            if ( (c == '\n' || c == ' '
                  || cprev == ',' || cprev =='.'
                  || cprev == '!' || cprev == '?')
                 //|| e==b)
                 && (c <= 591) ){

                // check if the text with the word gets in the line
                if ( (wordWidthtmp + l.bbox.getWidth()) < lineWidth){

                    // adds the word to the text
                    l.text.append(wordtmp);

                    // adds the word width to the text width
                    l.bbox.setWidth     (l.bbox.getWidth() + wordWidthtmp);

                    if (l.bottom < wordBottomtmp){
                        l.bbox.setHeight    (l.bbox.getHeight() + (wordBottomtmp - l.bottom));
                        l.bottom = wordBottomtmp;
                    }

                    // if the letter is bigger than 'x' (top)
                    if (l.top < wordToptmp){
                        l.bbox.setHeight    (l.bbox.getHeight() + (wordToptmp - l.top));
                        l.top = wordToptmp;
                    }
                }

                // word goes to the next line
                else if (wordWidthtmp < lineWidth){

                    // if the last character in the line was a space :
                    // erase the space as it was at the end of the line
#if defined(TARGET_WIN32) || defined(TARGET_LINUX)
                    if(l.text.back() == ' '){
#else
                    if(*l.text.rbegin() == ' '){
#endif
                        //if(l.text.back() == ' '){
                        l.text.erase    (l.text.end(),l.text.end());
                        l.bbox.setWidth (l.bbox.getWidth() - mImpl->spaceSize_);
                    }

                    // finish the line without adding the word
                    vecLines.push_back(l);

                    // start a new line containing the word
                    l       = startNewLine(l);
                    l.text  = wordtmp;
                    l.bbox.setWidth     (wordWidthtmp);
                    l.bbox.setHeight    (wordHeighttmp);
                    l.top       = wordToptmp;
                    l.bottom    = wordBottomtmp;
                }

                // word needs to be cut
                else{
                    std::vector<line> vecLinesWord = cutWords(wordtmp,lineWidth,
                                                              l.text, l.bbox.width, l.bbox.height,
                                                              l.bbox.position, l.bottom, l.top);
                    for (int k = 0;k< vecLinesWord.size()-1;k++){
                        vecLines.push_back(vecLinesWord[k]);
                    }
                    l = vecLinesWord[vecLinesWord.size()-1];
                }

                // clear word data
                wordtmp.clear();
                wordWidthtmp    = 0;
                wordHeighttmp   = 0;
                wordToptmp      = 0;
                wordBottomtmp   = 0;
                //vecLines.push_back({txttmp, widthtmp, postmp});
            }

            if(c == '\n'){

                // erase the space if it was at the end of the line
                if(cprev == ' '){

                    l.text.erase(l.text.end(),l.text.end());
                    l.bbox.setWidth (l.bbox.getWidth() - mImpl->spaceSize_);
                }
                vecLines.push_back(l);

                // start a new line
                l = startNewLine(l);
            }
            else if(c == ' '){
                cprev = c;
                // if the line with the space added is larger than 'width'
                if( l.bbox.getWidth() + mImpl->spaceSize_ > lineWidth ) {

                    // finish the line without adding the space
                    vecLines.push_back(l);

                    // start a new line
                    l = startNewLine(l);
                }
                else {

                    // ignore the space if it is the line beginning
                    if( l.bbox.getWidth() == 0){

                    }

                    else{
                        // adds the space to the line text
                        l.text.append(std::string(" "));

                        // adds the space width to the line width
                        l.bbox.setWidth (l.bbox.getWidth() + mImpl->spaceSize_);
                    }
                }
            }
            else {

                // latin character takes into account words
                if (c <= 591 || c==0x2019){ //right single quotation mark

                    if(l.xHeight == 0){

                        l.xHeight = mImpl->cps[mImpl->getCharID('x')].height;

                    }
                    if (wordHeighttmp == 0 ){

                        wordHeighttmp = l.xHeight;
                    }

                    if (l.bbox.height == 0){

                        l.bbox.height = l.xHeight;
                    }
                    // adds the letter to the word
                    utf8::append(c,back_inserter(wordtmp));

                    // adds the letter width to the word width
                    wordWidthtmp += mImpl->cps[cy].setWidth * mImpl->letterSpacing_;

                    // if the letter has pixels under the base line
                    int b = mImpl->cps[cy].topExtent - mImpl->cps[cy].height;

                    if (b > wordBottomtmp){

                        wordHeighttmp += (b - wordBottomtmp);
                        wordBottomtmp = b;
                    }
                    // if the letter is bigger than 'x' (top)
                    int t = mImpl->cps[cy].height - l.xHeight;

                    if (t > wordToptmp){

                        wordHeighttmp += (t - wordToptmp);
                        wordToptmp = t;
                    }
                }

                // non latin character
                else {
                    // if some special characters (number for instance) were parsed, add to the line
                    if(wordWidthtmp != 0)
                    {

                        // check if the text with the word gets in the line
                        if ( (wordWidthtmp + l.bbox.getWidth()) < lineWidth){

                            // adds the word to the text and its width to the line width
                            l.text.append(wordtmp);

                            l.bbox.setWidth     (l.bbox.getWidth()+ wordWidthtmp);

                            // if the letter has pixels under the base line
                            l.bbox.setHeight    (max(wordHeighttmp,(int)l.bbox.getHeight()));

                            l.top       = max(wordToptmp,l.top);
                            l.bottom    = max(wordBottomtmp,l.bottom);

                            // clear word data
                            wordtmp.clear();
                            wordWidthtmp    = 0;
                            wordHeighttmp   = 0;
                            wordToptmp      = 0;
                            wordBottomtmp   = 0;
                        }
                    }
                    bool punctuation = false;
                    if ( b!= e ){


                        int cnext   = utf8::peek_next (b, e);
                        int cnexty  = mImpl->getCharID(cnext);
                        if (mImpl->cps[cnexty].character == mImpl->kTypefaceUnloaded){
                            mImpl->loadCharInLargeTex(cnexty);
                        }
                        if(l.xHeight < mImpl->cps[cy].height)
                        {
                            l.bbox.setHeight(l.bbox.getHeight() + mImpl->cps[cy].height - l.xHeight);
                            l.xHeight = mImpl->cps[cy].height;

                        }
                        // if the letter has pixels under the base line
                        int b = mImpl->cps[cy].topExtent - mImpl->cps[cy].height;

                        if (l.bottom < b)
                        {
                            l.bbox.setHeight(l.bbox.getHeight() + b - l.bottom);
                            l.bottom += (b - l.bottom);
                        }
                        // if the letter is bigger than 'x' (top)
                       /* int t = mImpl->cps[cy].height - l.xHeight;

                        if (l.top < t)
                        {
                            l.bbox.setHeight(l.bbox.getHeight() + t - l.top);
                            l.top += (t - l.top);
                        }*/

                        // if the line with the new char and the next char (punctuation) added is larger than 'width'
                        if ((   cnext == 12289 // comma
                                || cnext == 12290 //fullstop
                                || cnext == 65288 || cnext == 65289 ) //opening and closing parenthesis
                                && (l.bbox.getWidth()
                                    + mImpl->cps[cy].setWidth * mImpl->letterSpacing_
                                    + mImpl->cps[cnexty].setWidth * mImpl->letterSpacing_> lineWidth
                                    )){

                            punctuation = true;
                            // finish the line without adding the new char
                            // and erasing the last character
                            vecLines.push_back(l);

                            l = startNewLine(l);
                            l = addNewChar(l,c);}
                    }

                    if(l.bbox.getWidth()
                            + mImpl->cps[cy].setWidth * mImpl->letterSpacing_ > lineWidth  && !punctuation){
                        // finish the line without adding the new char
                        vecLines.push_back(l);

                        // start a new line containing this char
                        l = startNewLine(l);
                        l = addNewChar(l,c);
                    }
                    else if(!punctuation){
                        l = addNewChar(l,c);
                    }
                }
            }
        }

        // end of line
        // adds the last line

        if (l.bbox.width != 0 && wordWidthtmp == 0){
            vecLines.push_back(l);
        }

        else  if (wordWidthtmp != 0 ){ // if there is a word not in the line yet
            // check if the text with the word gets in the line
            if ( (wordWidthtmp + l.bbox.getWidth()) < lineWidth){

                // adds the word to the text and its width to the line width
                l.text.append(wordtmp);

                l.bbox.setWidth     (l.bbox.getWidth()+ wordWidthtmp);

                // if the letter has pixels under the base line
                l.bbox.setHeight    (max(wordHeighttmp,(int)l.bbox.getHeight()));

                l.top       = max(wordToptmp,l.top);
                l.bottom    = max(wordBottomtmp,l.bottom);

                // adds the last line
                vecLines.push_back(l);
            }
            // word needs to be cut
            else{
                // erase the space if it was at the end of the line
#if defined(TARGET_WIN32) || defined(TARGET_LINUX)

                if(!l.text.empty() && l.text.back() == ' '){
#else
                if(*l.text.rbegin() == ' '){
#endif
                    //if(l.text.back() == ' '){

                    l.text.erase(l.text.end(),l.text.end());
                    l.bbox.setWidth (l.bbox.getWidth() - mImpl->spaceSize_);
                    vecLines.push_back(l);
                    l = startNewLine(l);

                }


                std::vector<line> vecLinesWord = cutWords(wordtmp, lineWidth,
                                                          l.text, l.bbox.getWidth(), l.bbox.getHeight(),
                                                          l.bbox.getPosition(), l.bottom, l.top);

                for (int k = 0;k< vecLinesWord.size();k++){
                    vecLines.push_back(vecLinesWord[k]);
                }
            }
        }
    }
    catch( utf8::invalid_utf8& e)
    {
        ofLog() << "CAUGHT EXCEPTION : " << e.what();
    }
    catch( utf8::not_enough_room& e)
    {
        ofLog() << "CAUGHT EXCEPTION : " << e.what();
    }

    // get the bbox corresponding to the alignType for each line
    for (int i = 0 ; i < vecLines.size() ; i++) {
        vecLines[i].bbox = getLineBoundingBox(vecLines[i], lineWidth,alignType);
    }

    multi.lines         = vecLines;
    multi.width         = lineWidth;
    multi.boundingBox   = getMultilineBoundingBox(vecLines);

    return multi;
}

ofRectangle ofxTrueTypeFontUC::getLineBoundingBox(const line &l, int width, ALIGN alignType){
    ofRectangle newbbox(l.bbox);

    /*if (l.top == 0)
    newbbox.setPosition(l.bbox.getPosition().x, l.bbox.getPosition().y );
    */
    switch (alignType){
    case LEFT: // default : align to the left
        // nothing to do
        break;
    case RIGHT: // align to the right
        newbbox.setPosition(l.bbox.getPosition().x + width - l.bbox.getWidth(),
                            l.bbox.getPosition().y);
        break;
    case CENTER: // align to the center
        newbbox.setPosition(l.bbox.getPosition().x + (width - l.bbox.getWidth())/2,
                            l.bbox.getPosition().y);
        break;
    default: // anything else = align to left
        // nothing to do
        break;
    }
    return newbbox;
}

ofRectangle ofxTrueTypeFontUC::getMultilineBoundingBox(const std::vector<line> &lines){
    ofRectangle res(0,0,0,0);
    if(lines.size()>0){
        for (int i = 0 ; i < lines.size() ; i++) {
            if(i==0){
                res = lines[i].bbox;
            }
            else{
                res = res.getUnion(lines[i].bbox);//getStringBoundingBox(lines[i].text, lines[i].position.x, lines[i].position.y));
            }
        }
    }
    return res;
}

//=====================================================================

void ofxTrueTypeFontUC::drawMultiline(const multiline &multi, bool baseLine){//, int lineWidth){//, ALIGN alignType){
    for (int i = 0 ; i < multi.lines.size() ; i++) {
        drawStringLine(multi.lines[i],i,baseLine);
    }
}
//=====================================================================

void ofxTrueTypeFontUC::drawMultilineAsShapes(const multiline &multi){//, int lineWidth){//, ALIGN alignType){
    for (int i = 0 ; i < multi.lines.size() ; i++) {
        drawLineAsShapes(multi.lines[i]);
    }
}
//=====================================================================

void ofxTrueTypeFontUC::drawLineAsShapes(const line &sline){//, int lineWidth){//, ALIGN alignType){
    if (!mImpl->bLoadedOk_) {
        ofLog(OF_LOG_ERROR,"ofxTrueTypeFontUC::drawString - Error : font not allocated -- line %d in %s", __LINE__,__FILE__);
        return;
    }

    float x = (int)sline.bbox.getPosition().x;
    float y;

    if(sline.xHeight != 0){    // latin character : y to the base line position

        y = (int) (sline.bbox.getPosition().y + sline.top + sline.xHeight);
    }
    else{ // if non latin character
        y = sline.bbox.getPosition().y + sline.bbox.getHeight() - sline.bottom ;
    }

    int c   = 0;
    int cy  = 0;

    std::string::const_iterator b = sline.text.begin();
    std::string::const_iterator e = sline.text.end();

    int pId = mImpl->getCharID('p');
    if (mImpl->cps[pId].character == mImpl->kTypefaceUnloaded)
        mImpl->loadCharInLargeTex(pId);

    setSpaceSize(mImpl->cps[pId].width);

    try{


        while(b != e){

            c =  utf8::next(b,e);

            if (c == ' ') {
                x += mImpl->spaceSize_ * mImpl->letterSpacing_ ;
            }
            else {

                cy = mImpl->getCharID(c);

                if (mImpl->cps[cy].character == mImpl->kTypefaceUnloaded)
                {
                    //mImpl->loadChar(cy);
                    mImpl->loadCharInLargeTex(cy);
                }

                //mImpl->drawCharLargeTex(cy,x,y);
                 mImpl->drawCharAsShape(cy,x,y);
                x += mImpl->cps[cy].setWidth * mImpl->letterSpacing_;
            }
        }
    }
    catch( utf8::invalid_utf8& e)
    {
        std::cerr<< "CAUGHT EXCEPTION : " << e.what() << std::endl;
    }
    catch( utf8::not_enough_room& e)
    {
        std::cerr<< "CAUGHT EXCEPTION : " << e.what() << std::endl;
    }
}

//=====================================================================

void ofxTrueTypeFontUC::drawStringLine(const line &sline, int index, bool baseLine){
    if (!mImpl->bLoadedOk_) {
        ofLog(OF_LOG_ERROR,"ofxTrueTypeFontUC::drawString - Error : font not allocated -- line %d in %s", __LINE__,__FILE__);
        return;
    }

    float x = (int)sline.bbox.getPosition().x;
    float y;

    if(baseLine) // y to the base line position
    {
        int offset = 0;
        if (index )
        {
          //  ofLog()<<sline.top + sline.xHeight;
          //  offset = sline.top + sline.xHeight;
        }
        y = (int) (sline.bbox.getPosition().y - offset);//-( sline.top + sline.xHeight));// - sline.bbox.getHeight());
    }
    else{// if(sline.xHeight != 0){

        y = (int) (sline.bbox.getPosition().y +( sline.top + sline.xHeight));
    }
   /* else{ // if non latin character
        y = sline.bbox.getPosition().y + sline.bbox.getHeight() - sline.bottom ;
    }*/

    int c   = 0;
    int cy  = 0;

    std::string::const_iterator b = sline.text.begin();
    std::string::const_iterator e = sline.text.end();

    int pId = mImpl->getCharID('p');
    if (mImpl->cps[pId].character == mImpl->kTypefaceUnloaded)
        mImpl->loadCharInLargeTex(pId);

    setSpaceSize(mImpl->cps[pId].width);

    try{

        mImpl->bindLargeTex();

        while(b != e){

            c =  utf8::next(b,e);

            if (c == ' ' || c == 0xA0) {
                x += mImpl->spaceSize_ * mImpl->letterSpacing_ ;
            }
            else {

                cy = mImpl->getCharID(c);

                if (mImpl->cps[cy].character == mImpl->kTypefaceUnloaded)
                {
                    //mImpl->loadChar(cy);
                    mImpl->loadCharInLargeTex(cy);
                }

                mImpl->drawCharLargeTex(cy,x,y);

                x += mImpl->cps[cy].setWidth * mImpl->letterSpacing_;
            }
        }
        mImpl->unbindLargeTex();
    }
    catch( utf8::invalid_utf8& e)
    {
        std::cerr<< "CAUGHT EXCEPTION : " << e.what() << std::endl;
    }
    catch( utf8::not_enough_room& e)
    {
        std::cerr<< "CAUGHT EXCEPTION : " << e.what() << std::endl;
    }


}
//=====================================================================

int ofxTrueTypeFontUC::getNbWordCut(const multiline &multi){//, int lineWidth){//, ALIGN alignType){
    int res = 0;
    for (int i = 0 ; i < multi.lines.size() ; i++) {
#if defined(TARGET_WIN32) || defined(TARGET_LINUX)
        if(!multi.lines[i].text.empty() && multi.lines[i].text.back() == '-')
#else
        if(!multi.lines[i].text.empty() && *multi.lines[i].text.rbegin() == '-')
#endif
            res ++;
    }

    return res;
}
//=====================================================================
void ofxTrueTypeFontUC::bindLargeTex(){
    mImpl->bindLargeTex();
}

//=====================================================================
void ofxTrueTypeFontUC::unbindLargeTex(){
    mImpl->unbindLargeTex();
}
//=====================================================================

//=====================================================================
void ofxTrueTypeFontUC::drawString(const std::string &src, float x, float y,bool bindTex){
    if (!mImpl->bLoadedOk_) {
        ofLog(OF_LOG_ERROR,"ofxTrueTypeFontUC::drawString - Error : font not allocated -- line %d in %s", __LINE__,__FILE__);
        return;
    }

    int c   = 0;
    int cy  = 0;

    std::string::const_iterator b = src.begin();
    std::string::const_iterator e = src.end();

    int pId = mImpl->getCharID('p');
    if (mImpl->cps[pId].character == mImpl->kTypefaceUnloaded)
        mImpl->loadCharInLargeTex(pId);

    setSpaceSize(mImpl->cps[pId].width);

    try{
        if(bindTex)
            mImpl->bindLargeTex();

        while(b != e){

            c =  utf8::next(b,e);

            if (c == ' '  || c == 0xA0) {
                x += mImpl->spaceSize_ * mImpl->letterSpacing_ ;
            }
            else {

                cy = mImpl->getCharID(c);

                if (mImpl->cps[cy].character == mImpl->kTypefaceUnloaded)
                {
                    mImpl->loadCharInLargeTex(cy);
                }

                mImpl->drawCharLargeTex(cy,x,y);

                x += mImpl->cps[cy].setWidth * mImpl->letterSpacing_;
            }
        }
        if(bindTex)
            mImpl->unbindLargeTex();
    }
    catch( utf8::invalid_utf8& e)
    {
        std::cerr<< "CAUGHT EXCEPTION : " << e.what() << std::endl;
    }
    catch( utf8::not_enough_room& e)
    {
        std::cerr<< "CAUGHT EXCEPTION : " << e.what() << std::endl;
    }


}
//=====================================================================
ofRectangleUC ofxTrueTypeFontUC::getStringBoundingBox(const std::string &str, float x, float y){
    ofRectangleUC myRect;

    if (!mImpl->bLoadedOk_) {
        ofLog(OF_LOG_ERROR,"ofxTrueTypeFontUC::getStringBoundingBox - font not allocated");
        return myRect;
    }
    GLfloat xoffset	= 0;
    GLfloat yoffset	= 0;
    float minx = -1;
    float miny = -1;
    float maxx = -1;
    float maxy = -1;
    int c      = 0;
    int cy     = 0;

    try{

        std::string::const_iterator b = str.begin();
        std::string::const_iterator e = str.end();

        const int len = utf8::distance(b,e);



        int pId = mImpl->getCharID('p');

        if (mImpl->cps[pId].character == mImpl->kTypefaceUnloaded){
            mImpl->loadCharInLargeTex(pId);
            setSpaceSize(mImpl->cps[pId].width);
        }
        if (len < 1 || mImpl->cps.empty()) {

            myRect.x = 0;
            myRect.y = 0;
            myRect.width = 0;
            myRect.height = 0;
            return myRect;
        }

        bool bFirstCharacter = true;



        while(b != e){

            c =  utf8::next(b,e);

            if (c == '\n') {
                yoffset += mImpl->lineHeight_;
                xoffset = 0 ; //reset X Pos back to zero
            }
            else if (c == ' ') {

                xoffset +=  mImpl->spaceSize_ * mImpl->letterSpacing_;
            }
            else {
                cy = mImpl->getCharID(c);
                if (mImpl->cps[cy].character == mImpl->kTypefaceUnloaded){
                    mImpl->loadCharInLargeTex(cy);
                }
                //mImpl->drawCharLargeTex(cy,xoffset,yoffset);
                GLint top = mImpl->cps[cy].topExtent - mImpl->cps[cy].height;

                if(top>myRect.top)
                    myRect.top = abs(top);

                float	x1, y1, x2, y2;

                x1  = mImpl->cps[cy].x1 + x + xoffset;
                y1  = mImpl->cps[cy].y1 + y + yoffset;
                x2  = mImpl->cps[cy].x2 + x + xoffset;
                y2  = mImpl->cps[cy].y2 + y + yoffset;

                xoffset += mImpl->cps[cy].setWidth * mImpl->letterSpacing_;

                if (bFirstCharacter == true) {
                    minx = x2;
                    miny = y2;
                    maxx = x1;
                    maxy = y1;
                    bFirstCharacter = false;
                }
                else {
                    if (x2 < minx)
                        minx = x2;
                    if (y2 < miny)
                        miny = y2;
                    if (x1 > maxx)
                        maxx = x1;
                    if (y1 > maxy)
                        maxy = y1;
                }
            }
        }
    }
    catch( utf8::invalid_utf8& e)
    {
        std::cerr<< "CAUGHT EXCEPTION : " << e.what() << std::endl;
    }
    catch( utf8::not_enough_room& e)
    {
        std::cerr<< "CAUGHT EXCEPTION : " << e.what() << std::endl;
    }

    myRect.x = minx;
    myRect.y = miny;
    myRect.width = maxx-minx;
    myRect.height = maxy-miny;
    return myRect;
}

//=====================================================================
float ofxTrueTypeFontUC::getLinePositionY(const multiline &multi, int lineNb, bool baseLine)
{
    if(lineNb > (multi.lines.size()-1) || lineNb < 0)
        return 0.f;
    float res = multi.boundingBox.getPosition().y + lineNb * mImpl->lineHeight_;
    if(baseLine)
        res += mImpl->lineHeight_;
    return res;

}
//=====================================================================
float ofxTrueTypeFontUC::getLastLinePositionY(const multiline &multi, bool baseLine)
{
    int lastLineNb = multi.lines.size()-1;

    return getLinePositionY(multi,lastLineNb,baseLine);

}
//=====================================================================

//=====================================================================


//=====================================================================
void ofxTrueTypeFontUC::drawStringAsShapes(const string &src, float x, float y){

    if (!mImpl->bLoadedOk_) {
        ofLogError("ofxTrueTypeFontUC") << "drawStringAsShapes(): font not allocated: line " << __LINE__ << " in " << __FILE__;
        return;
    }

    //----------------------- error checking
    if (!mImpl->bMakeContours_) {
        ofLog(OF_LOG_ERROR,"ofxTrueTypeFontUC::drawStringAsShapes - Error : contours not created for this font - call loadFont with makeContours set to true");
        return;
    }

    GLfloat X   = x;
    GLfloat Y   = y;
    int c       = 0;
    int cy      = 0;

    std::string::const_iterator b = src.begin();
    std::string::const_iterator e = src.end();


    try{

        while(b != e){

            c =  utf8::next(b,e);

            if (c == '\n') {
                Y += mImpl->lineHeight_;
                X = x ; //reset X Pos back to zero
            }
            else if (c == ' ') {
                cy = mImpl->getCharID('p');
                X += mImpl->cps[cy].width;
            }
            else {
                cy = mImpl->getCharID(c);
                if (mImpl->cps[cy].character == mImpl->kTypefaceUnloaded)
                    mImpl->loadCharInLargeTex(cy);
                mImpl->drawCharAsShape(cy, X, Y);
                X += mImpl->cps[cy].setWidth;
            }
        }
    }
    catch( utf8::invalid_utf8& e)
    {
        std::cerr<< "CAUGHT EXCEPTION : " << e.what() << std::endl;
    }
    catch( utf8::not_enough_room& e)
    {
        std::cerr<< "CAUGHT EXCEPTION : " << e.what() << std::endl;
    }

}

//-----------------------------------------------------------
int ofxTrueTypeFontUC::getLoadedCharactersCount() {
    return mImpl->loadedChars.size();
}

//=====================================================================
const int ofxTrueTypeFontUC::Impl::kTypefaceUnloaded = 0;
const int ofxTrueTypeFontUC::Impl::kDefaultLimitCharactersNum = 10000;

//-----------------------------------------------------------
void ofxTrueTypeFontUC::Impl::bind(const int & charID) {
    if (!binded_) {
        // we need transparency to draw text, but we don't know
        // if that is set up in outside of this function
        // we "pushAttrib", turn on alpha and "popAttrib"
        // http://www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/pushattrib.html

        // **** note ****
        // I have read that pushAttrib() is slow, if used often,
        // maybe there is a faster way to do this?
        // ie, check if blending is enabled, etc...
        // glIsEnabled().... glGet()...
        // http://www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/get.html
        // **************
        // (a) record the current "alpha state, blend func, etc"
#ifndef TARGET_OPENGLES
        glPushAttrib(GL_COLOR_BUFFER_BIT);
#else
        blend_enabled_ = glIsEnabled(GL_BLEND);
        texture_2d_enabled_ = glIsEnabled(GL_TEXTURE_2D);
        glGetIntegerv( GL_BLEND_SRC, &blend_src);
        glGetIntegerv( GL_BLEND_DST, &blend_dst_ );
#endif

        // (b) enable our regular ALPHA blending!
        if (m_bHasCustomBlending == false)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        textures[charID].bind();
        stringQuads.clear();
        binded_ = true;
    }
}

//-----------------------------------------------------------
void ofxTrueTypeFontUC::Impl::unbind(const int & charID) {
    if (binded_) {
        stringQuads.drawFaces();

        textures[charID].unbind();

#ifndef TARGET_OPENGLES
        glPopAttrib();
#else
        if (!blend_enabled_)
            glDisable(GL_BLEND);
        if  (!texture_2d_enabled_)
            glDisable(GL_TEXTURE_2D);
        glBlendFunc( blend_src, blend_dst_ );
#endif
        binded_ = false;
    }
}
//-----------------------------------------------------------
void ofxTrueTypeFontUC::Impl::bindLargeTex() {
    if (!binded_) {
        // we need transparency to draw text, but we don't know
        // if that is set up in outside of this function
        // we "pushAttrib", turn on alpha and "popAttrib"
        // http://www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/pushattrib.html

        // **** note ****
        // I have read that pushAttrib() is slow, if used often,
        // maybe there is a faster way to do this?
        // ie, check if blending is enabled, etc...
        // glIsEnabled().... glGet()...
        // http://www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/get.html
        // **************
        // (a) record the current "alpha state, blend func, etc"
#ifndef TARGET_OPENGLES
        glPushAttrib(GL_COLOR_BUFFER_BIT);
#else
        blend_enabled_ = glIsEnabled(GL_BLEND);
        texture_2d_enabled_ = glIsEnabled(GL_TEXTURE_2D);
        glGetIntegerv( GL_BLEND_SRC, &blend_src);
        glGetIntegerv( GL_BLEND_DST, &blend_dst_ );
#endif

        // (b) enable our regular ALPHA blending!
        if (m_bHasCustomBlending == false)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        largeTex.bind();
        stringQuads.clear();
        binded_ = true;
    }
}

//-----------------------------------------------------------
void ofxTrueTypeFontUC::Impl::unbindLargeTex() {
    if (binded_) {
        largeTex.unbind();

#ifndef TARGET_OPENGLES
        glPopAttrib();
#else
        if (!blend_enabled_)
            glDisable(GL_BLEND);
        if  (!texture_2d_enabled_)
            glDisable(GL_TEXTURE_2D);
        glBlendFunc( blend_src, blend_dst_ );
#endif
        binded_ = false;
    }
}
//-----------------------------------------------------------
int ofxTrueTypeFontUC::getLimitCharactersNum() {
    return mImpl->limitCharactersNum_;
}

//-----------------------------------------------------------
void ofxTrueTypeFontUC::reserveCharacters(int charactersNumber) {
    mImpl->implReserveCharacters(charactersNumber);
}

//-----------------------------------------------------------
void ofxTrueTypeFontUC::setCustomBlending(bool is) 
{
    mImpl->setCustomBlending(is);
}

//-----------------------------------------------------------
void ofxTrueTypeFontUC::Impl::setCustomBlending(bool is) 
{
    m_bHasCustomBlending = is;
}

//-----------------------------------------------------------
void ofxTrueTypeFontUC::Impl::implReserveCharacters(int num) {
    if (num <= 0)
        return;

    limitCharactersNum_ = num;

    vector<charPropsUC>().swap(cps);
    vector<ofTexture>().swap(textures);
    vector<int>().swap(loadedChars);
    vector<ofPath>().swap(charOutlines);

    //--------------- initialize character info and textures
    cps.resize(limitCharactersNum_);
    for (int i=0; i<limitCharactersNum_; ++i)
        cps[i].character = kTypefaceUnloaded;
    textures.resize(limitCharactersNum_);

    if (bMakeContours_) {
        charOutlines.clear();
        charOutlines.assign(limitCharactersNum_, ofPath());
    }

    //--------------- load 'p' character for display ' '
    /*     int cy = getCharID('p');

    loadChar(cy);
    loadCharInLargeTex(cy);
    */
}

//-----------------------------------------------------------
int ofxTrueTypeFontUC::Impl::getCharID(const int &c) {
    int tmp = (int)c;
    int point = 0;
    for (; point != (int)loadedChars.size(); ++point) {
        if (loadedChars[point] == tmp) {
            break;
        }
    }
    if (point == loadedChars.size()) {
        //----------------------- error checking
        if (point >= limitCharactersNum_) {
            ofLog(OF_LOG_ERROR,"ofxTrueTypeFontUC::getCharID - Error : too many typeface already loaded - call loadFont to reset");
            return point = 0;
        }
        else {
            loadedChars.push_back(tmp);
        }
    }
    return point;
}

//-----------------------------------------------------------
void ofxTrueTypeFontUC::Impl::loadChar(const int &charID) {
    int i = charID;
    ofPixels expandedData;


    //------------------------------------------ anti aliased or not:
    FT_Error err = FT_Load_Glyph( face_, FT_Get_Char_Index( face_, loadedChars[i] ), FT_LOAD_DEFAULT );
    if(err)
        ofLog(OF_LOG_ERROR,"ofxTrueTypeFontUC::loadFont - Error with FT_Load_Glyph %i: FT_Error = %d", loadedChars[i], err);

    if (bAntiAliased_ == true)
        FT_Render_Glyph(face_->glyph, FT_RENDER_MODE_NORMAL);
    else
        FT_Render_Glyph(face_->glyph, FT_RENDER_MODE_MONO);

    //------------------------------------------
    FT_Bitmap& bitmap= face_->glyph->bitmap;

    // prepare the texture:
    /*int width  = ofNextPow2( bitmap.width + border*2 );
    int height = ofNextPow2( bitmap.rows  + border*2 );


    // ------------------------- this is fixing a bug with small type
    // ------------------------- appearantly, opengl has trouble with
    // ------------------------- width or height textures of 1, so we
    // ------------------------- we just set it to 2...
    if (width == 1) width = 2;
    if (height == 1) height = 2;*/

    if (bMakeContours_) {
        if (printVectorInfo_)
            printf("\n\ncharacter charID %d: \n", i );

        charOutlines[i] = makeContoursForCharacter(face_);
        if (simplifyAmt_>0)
            charOutlines[i].simplify(simplifyAmt_);
        charOutlines[i].getTessellation();
    }

    // -------------------------
    // info about the character:
    cps[i].character = loadedChars[i];
    cps[i].height = face_->glyph->bitmap_top;
    cps[i].width = face_->glyph->bitmap.width;
    cps[i].setWidth = face_->glyph->advance.x >> 6;
    cps[i].topExtent = face_->glyph->bitmap.rows;
    cps[i].leftExtent = face_->glyph->bitmap_left;

    int width = cps[i].width;
    int height = bitmap.rows;

    cps[i].tW = width;
    cps[i].tH = height;

    GLint fheight = cps[i].height;
    GLint bwidth = cps[i].width;
    GLint top = cps[i].topExtent - cps[i].height;
    GLint lextent	= cps[i].leftExtent;

    GLfloat	corr, stretch;

    //this accounts for the fact that we are showing 2*visibleBorder extra pixels
    //so we make the size of each char that many pixels bigger
    stretch = 0;

    corr	= (float)(((fontSize_ - fheight) + top) - fontSize_);

    cps[i].x1 = lextent + bwidth + stretch;
    cps[i].y1 = fheight + corr + stretch;
    cps[i].x2 = (float) lextent;
    cps[i].y2 = -top + corr;

    // Allocate Memory For The Texture Data.
    expandedData.allocate(width, height, 4); //2
    //-------------------------------- clear data:
    expandedData.set(0,255); // every luminance pixel = 255
    //expandedData.set(1,0);
    expandedData.set(1,255);
    expandedData.set(2,255);
    expandedData.set(3,0);

    if (bAntiAliased_ == true) {
        ofPixels bitmapPixels;
        //   bitmapPixels.allocate(bitmap.width,bitmap.rows,4);
        // bitmapPixels.setFromExternalPixels(bitmap.buffer,bitmap.width,bitmap.rows,4);

        bitmapPixels.setFromExternalPixels(bitmap.buffer,bitmap.width,bitmap.rows,1);
        //expandedData.setChannel(1,bitmapPixels);
        expandedData.setChannel(3,bitmapPixels);
    }
    else {
        //-----------------------------------
        // true type packs monochrome info in a
        // 1-bit format, hella funky
        // here we unpack it:
        unsigned char *src =  bitmap.buffer;
        for(int j=0; j<bitmap.rows; ++j) {
            unsigned char b=0;
            unsigned char *bptr =  src;
            for(int k=0; k<bitmap.width; ++k){
                expandedData[2*(k+j*width)] = 255;

                if (k%8==0)
                    b = (*bptr++);

                expandedData[2*(k+j*width) + 1] = b&0x80 ? 255 : 0;
                b <<= 1;
            }
            src += bitmap.pitch;
        }
        //-----------------------------------
    }

    int longSide = border_ * 2;
    cps[i].tW > cps[i].tH ? longSide += cps[i].tW : longSide += cps[i].tH;

    int tmp = 1;
    while (longSide > tmp) {
        tmp <<= 1;
    }
    int w = tmp;
    int h = w;

    ofPixels atlasPixels;
    atlasPixels.allocate(w,h,4); ///2
    // atlasPixels.set(1,0);
    atlasPixels.set(0,255);
    atlasPixels.set(1,255);
    atlasPixels.set(2,255);
    atlasPixels.set(3,0);

    cps[i].t2 = float(border_) / float(w);
    cps[i].v2 = float(border_) / float(h);
    cps[i].t1 = float(cps[i].tW + border_) / float(w);
    cps[i].v1 = float(cps[i].tH + border_) / float(h);
    expandedData.pasteInto(atlasPixels, border_, border_);

    textures[i].allocate(atlasPixels.getWidth(), atlasPixels.getHeight(), GL_RGBA, false);

    if (bAntiAliased_ && fontSize_>20) {
        textures[i].setTextureMinMagFilter(GL_LINEAR,GL_LINEAR);
    }
    else {
        textures[i].setTextureMinMagFilter(GL_NEAREST,GL_NEAREST);
    }

    textures[i].loadData(atlasPixels.getData(), atlasPixels.getWidth(), atlasPixels.getHeight(), GL_RGBA);

}
//-----------------------------------------------------------
void ofxTrueTypeFontUC::Impl::loadCharInLargeTex(const int &charID) {

    int i = charID;
    ofPixels expandedData;

    //------------------------------------------ anti aliased or not:
    FT_Error err = FT_Load_Glyph( face_, FT_Get_Char_Index( face_, loadedChars[i] ), FT_LOAD_DEFAULT );
    if(err)
        ofLog(OF_LOG_ERROR,"ofxTrueTypeFontUC::loadFont - Error with FT_Load_Glyph %i: FT_Error = %d", loadedChars[i], err);

    if (bAntiAliased_ == true)
        FT_Render_Glyph(face_->glyph, FT_RENDER_MODE_NORMAL);
    else
        FT_Render_Glyph(face_->glyph, FT_RENDER_MODE_MONO);

    //------------------------------------------
    FT_Bitmap& bitmap= face_->glyph->bitmap;

    if (bMakeContours_) {
        if (printVectorInfo_)
            printf("\n\ncharacter charID %d: \n", i );

        charOutlines[i] = makeContoursForCharacter(face_);
        if (simplifyAmt_>0)
            charOutlines[i].simplify(simplifyAmt_);
        charOutlines[i].getTessellation();
    }

    // -------------------------
    // info about the character:
    cps[i].character   = loadedChars[i];

    cps[i].height      = face_->glyph->bitmap_top;
    cps[i].width       = face_->glyph->bitmap.width;
    cps[i].setWidth    = face_->glyph->advance.x >> 6;
    cps[i].topExtent   = face_->glyph->bitmap.rows;
    cps[i].leftExtent  = face_->glyph->bitmap_left;

    int width   = cps[i].width;
    int height  = bitmap.rows;

    cps[i].tW = width;
    cps[i].tH = height;

    GLint fheight   = cps[i].height;
    GLint bwidth    = cps[i].width;
    GLint top       = cps[i].topExtent - cps[i].height;
    GLint lextent   = cps[i].leftExtent;

    GLfloat	corr, stretch;

    //this accounts for the fact that we are showing 2*visibleBorder extra pixels
    //so we make the size of each char that many pixels bigger
    stretch = 0;

    corr	= (float)(((fontSize_ - fheight) + top) - fontSize_);

    cps[i].x1 = lextent + bwidth + stretch;
    cps[i].y1 = fheight + corr + stretch;
    cps[i].x2 = (float) lextent;
    cps[i].y2 = -top + corr;

    // Allocate Memory For The Texture Data.
    expandedData.allocate(width, height, 4);
    //-------------------------------- clear data:
    expandedData.set(0,255);
    expandedData.set(1,255);
    expandedData.set(2,255);
    expandedData.set(3,0);

    if (bAntiAliased_ == true) {
        ofPixels bitmapPixels;
        bitmapPixels.setFromExternalPixels(bitmap.buffer,bitmap.width,bitmap.rows,1);
        expandedData.setChannel(3,bitmapPixels);
    }
    else {
        //-----------------------------------
        // true type packs monochrome info in a
        // 1-bit format, hella funky
        // here we unpack it:
        unsigned char *src =  bitmap.buffer;
        for(int j=0; j<bitmap.rows; ++j) {
            unsigned char b=0;
            unsigned char *bptr =  src;
            for(int k=0; k<bitmap.width; ++k){
                expandedData[2*(k+j*width)] = 255;

                if (k%8==0)
                    b = (*bptr++);

                expandedData[2*(k+j*width) + 1] = b&0x80 ? 255 : 0;
                b <<= 1;
            }
            src += bitmap.pitch;
        }
        //-----------------------------------
    }

    int w = cps[i].tW + 2*border_;
    int h = cps[i].tH + 2*border_;

    ofPixels atlasPixels;
    atlasPixels.allocate(w,h,4);
    atlasPixels.set(0,255);
    atlasPixels.set(1,255);
    atlasPixels.set(2,255);
    atlasPixels.set(3,0);

    cps[i].t2 = float(border_) / float(w);
    cps[i].v2 = float(border_) / float(h);
    cps[i].t1 = float(cps[i].tW + border_) / float(w);
    cps[i].v1 = float(cps[i].tH + border_) / float(h);
    expandedData.pasteInto(atlasPixels, border_, border_);

    // allocating large texture

    if(!largeTex.isAllocated()){
        if (fontSize_ > 30)
            largeTex.allocate(1024,1024, GL_RGBA, false);
        else
            largeTex.allocate(512,512, GL_RGBA, false);

        if (bAntiAliased_ && fontSize_>20) {

            largeTex.setTextureMinMagFilter(GL_LINEAR,GL_LINEAR);
        }
        else {
            largeTex.setTextureMinMagFilter(GL_NEAREST,GL_NEAREST);
        }
    }

    // load character in the large texture
    if ( _yoffset + atlasPixels.getHeight() < largeTex.getHeight()){ //(i==18  && !img.isAllocated()){

        if(_xoffset + atlasPixels.getWidth() >= largeTex.getWidth()){

            _yoffset    += _heightMax;
            _xoffset    = 10;
        }

        // set tex coord
        cps[i].t2 = float(_xoffset + border_) / float(largeTex.getWidth());
        cps[i].v2 = float(_yoffset + border_) / float(largeTex.getHeight());
        cps[i].t1 = float(cps[i].tW + border_ + _xoffset) / float(largeTex.getWidth());
        cps[i].v1 = float(cps[i].tH + border_ + _yoffset) / float(largeTex.getHeight());

        loadData(atlasPixels.getData(), _xoffset,_yoffset, atlasPixels.getWidth(), atlasPixels.getHeight(),  GL_RGBA);

        // compute offset for the next letter
        _heightMax = max(_heightMax, atlasPixels.getHeight());
        _xoffset += atlasPixels.getWidth() ;
    }

    // Save only one character
    /* bool saveOneChar = false;
    int charNum = 2;

    if (saveOneChar && i ==  charNum) {

    img.allocate(atlasPixels.getWidth(), atlasPixels.getHeight(),OF_IMAGE_COLOR_ALPHA);

    img.setFromPixels(atlasPixels);

    img.save("a.png");
    }*/


    // Saving texture to an image by getting the pixels
    /*bool saveTex =  false;

    if (i == 5 && saveTex){ //last character to be loaded

    ofPixels pix;
    pix.allocate(largeTex.getWidth(),largeTex.getHeight(),4);

    pix.set (0,255);
    pix.set (1,255);
    pix.set (2,255);
    pix.set (3,0);

    largeTex.readToPixels(pix);

    img.allocate(pix.getWidth(), pix.getHeight(),OF_IMAGE_COLOR_ALPHA);

    img.setFromPixels(pix);

    img.save("testLargeTexbis.png");
    }*/
}

//-----------------------------------

void ofxTrueTypeFontUC::Impl::loadData(const void * data,  int x, int y, int w, int h, int glFormat, int glType) {
    //if the line below does not work use the commented line instead
    ofSetPixelStoreiAlignment(GL_UNPACK_ALIGNMENT,w,1,ofGetNumChannelsFromGLFormat(glFormat));
    //ofSetPixelStorei(w,1,ofGetNumChannelsFromGLFormat(glFormat));

    // allocate the large texture
    if(w > largeTex.texData.tex_w || h >  largeTex.texData.tex_h) {
        if(largeTex.isAllocated()){
            largeTex.allocate(w, h,  glFormat, glFormat, glType);
        }else{
            largeTex.allocate(w, h,  glFormat, glFormat, glType);
        }
    }

    // compute new tex co-ords based on the ratio of data's w, h to texture w,h;
#ifndef TARGET_OPENGLES
    if ( largeTex.texData.textureTarget == GL_TEXTURE_RECTANGLE_ARB){
        largeTex.texData.tex_t = w;
        largeTex.texData.tex_u = h;
    } else
#endif
    {
        largeTex.texData.tex_t = (float)(w) / (float) largeTex.texData.tex_w;
        largeTex.texData.tex_u = (float)(h) / (float) largeTex.texData.tex_h;
    }
    // bind texture
    // glEnable(largeTex.texData.textureTarget);
    glBindTexture( largeTex.texData.textureTarget, (GLuint)  largeTex.texData.textureID);
    //update the texture image:
    glTexSubImage2D( largeTex.texData.textureTarget, 0, x,y, w,h, glFormat, glType, data);
    // unbind texture target by binding 0
    glBindTexture( largeTex.texData.textureTarget, 0);

    /*if (largeTex.bWantsMipmap) {
    // auto-generate mipmap, since this ofTexture wants us to.
    largeTex.generateMipmap();
    }*/

}
