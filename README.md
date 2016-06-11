# ofxTrueTypeFontUC

## Overview

Fork from ofxTrueTypeFont.

Tested on OSX, Linux (Debian Jessie), and Windows 7.
For installation, read the README of the origin repository.

## Modifications

* Removed the conversion method (UTF16->UTF-8->UTF-32). To parse the string character by character, the lib ```utf8``` is used instead.

* Texture system has changed : instead of loading every character in a different texture (which can be slow to display if you have a long text), it is loaded in a single large texture.

## Added features

* Multiline support : given a string, line width, a position, the method ```parseText``` parses and saves the string into a multiline structure which you can draw with the method ```drawMultiline```. It is also possible to align the text to the left/right/center (last parameter of parseText, the default value is left alignment).
An example can be find in ```example/src/testApp.cpp```)

