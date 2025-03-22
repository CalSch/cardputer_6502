#include "asm_editor.h"

void editorSave() {
  assembler.input_lines = editor.code;
}

void editorDraw() {
  M5Cardputer.Display.clearDisplay();
  char charStr[2]="h";
  for (int y=0;y<TXT_ROWS;y++) {
    int rowIdx=y+editor.scrollY;
    if (rowIdx<0 || rowIdx>(int)editor.code.size()-1)
      continue;
    M5Cardputer.Display.setTextColor(TFT_SILVER);
    M5Cardputer.Display.drawString(format_text("%3d",rowIdx+1),0,y*FNT_H);
    M5Cardputer.Display.setTextColor(TFT_GREEN);
    std::string line = editor.code[rowIdx];
    Serial.printf("code[%d]='%s' (%d)\n",rowIdx,line.c_str(),editor.code.size());
    for (int x=0;x<TXT_COLS;x++) {
      int charIdx=x+editor.scrollX;
      if (rowIdx==editor.cursorY && charIdx==editor.cursorX) {
        M5Cardputer.Display.drawString("_",(x+3)*FNT_W+4,y*FNT_H);
      }
      if (charIdx > (int)line.size()-1) {
        continue;
      }
      char c=line[charIdx];
      charStr[0]=line[charIdx];
      Serial.printf(" line[%d]='%c'='%s' (%d)\n",charIdx,line[charIdx],charStr,line.size());
      M5Cardputer.Display.setTextColor(TFT_RED);
      if (c>='0' && c<='9') M5Cardputer.Display.setTextColor(TFT_YELLOW);
      if ((c>='a' && c<='z') || (c>='A' && c<='Z')) M5Cardputer.Display.setTextColor(TFT_SKYBLUE);
      M5Cardputer.Display.drawString(charStr,(x+3)*FNT_W+4,y*FNT_H);
    }
  }
  M5Cardputer.Display.drawFastVLine(FNT_W*3+2,0,SCR_H,TFT_WHITE);
}

void editorClampCursorX() {
  editor.cursorX=min(max(editor.cursorX,0),(int)editor.code[editor.cursorY].size());
}
void editorUpdateScroll() {
  int minScroll=editor.cursorY-1;
  int maxScroll=editor.cursorY+TXT_ROWS;
  if (minScroll<editor.scrollY) {
    editor.scrollY=minScroll;
  }
  if (editor.cursorY>editor.scrollY+TXT_ROWS-2) {
    editor.scrollY=editor.cursorY-TXT_ROWS+2;
  }
  editor.scrollY=min(max(editor.scrollY,0),(int)editor.code.size());
}

void editorOnKeyPress(char c) {
  Serial.printf("key: %c\n",c);
  editor.code[editor.cursorY].insert(editor.code[editor.cursorY].begin()+editor.cursorX,c);
  editor.cursorX++;
}
void editorOnDeletePress() {
  std::string line = editor.code[editor.cursorY];
  if (editor.cursorX==0) {
    if (editor.cursorY>0) {
      editor.cursorX=editor.code[editor.cursorY-1].size();
      editor.code[editor.cursorY-1]+=line;
      editor.code.erase(editor.code.begin()+editor.cursorY);
      editor.cursorY--;
    }
  } else {
    editor.cursorX--;
    editor.code[editor.cursorY].erase(editor.cursorX,1);
  }
}
void editorOnEnterPress() {
  editor.code.insert(editor.code.begin()+editor.cursorY,std::string(""));
  editor.cursorY++;
}
bool editorUp() { // returns true if already at the top
  editor.cursorY--;
  if (editor.cursorY<0) {
    editor.cursorY=0;
    editor.cursorX=0;
    return true;
  }
  return false;
}
bool editorDown() { // returns true if already at the bottom
  editor.cursorY++;
  if (editor.cursorY>=editor.code.size()) {
    editor.cursorY=editor.code.size()-1;
    editor.cursorX=editor.code[editor.cursorY].size();
    return true;
  }
  return false;
}
void editorLeft() {
  editor.cursorX--;
  if (editor.cursorX<0) {
    if (!editorUp()) editor.cursorX=editor.code[editor.cursorY].size();
  }
}
void editorRight() {
  editor.cursorX++;
  if (editor.cursorX>editor.code[editor.cursorY].size()) {
    editor.cursorX=0;
    editorDown();
  }
}
void editorOnFnKeyPress(char c) {
  switch (c) {
    case ';':
      editorUp();
      editorClampCursorX();
      editorUpdateScroll();
      break;
    case '.':
      editorDown();
      editorClampCursorX();
      editorUpdateScroll();
      break;
    case ',':
      editorLeft();
      editorClampCursorX();
      editorUpdateScroll();
      break;
    case '/':
      editorRight();
      editorClampCursorX();
      editorUpdateScroll();
      break;
  }
}
void editorOnCtrlKeyPress(char c) {
  switch (c) {
    case 'S':
      editorSave();
      break;
  }
}

int editorVectorFind(std::vector<char> v, char c) {
  for (int i=0;i<v.size();i++) {
    if (v[i]==c)
      return i;
  }
  return -1;
}
static std::vector<char> editorlastKeysPressed;
void editorUpdate(bool updateKB) {

  M5Cardputer.update();
  if (updateKB) {
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
    
    for (char c : status.word) {
      int idx = editorVectorFind(editorlastKeysPressed, tolower(c));
      if (idx==-1) {
        if (status.ctrl)
          editorOnCtrlKeyPress(c);
        else if (status.fn)
          editorOnFnKeyPress(c);
        // else if (status.opt)
        //   onOptKeyPress(c);
        else
          editorOnKeyPress(c);
        editorlastKeysPressed.push_back(c);
      }
    }
    if (status.enter) {
      editorOnEnterPress();
    }
    if (status.del) {
      editorOnDeletePress();
    }
    editorDraw();
  }
  for (int i=0;i<editorlastKeysPressed.size();i++) {
    if (!M5Cardputer.Keyboard.isKeyPressed(editorlastKeysPressed[i])) {
      editorlastKeysPressed.erase(editorlastKeysPressed.begin()+i);
    }
  }
  
  // M5Cardputer.Display.fillRect(10,10,20,30);
  

  delay(50);
}