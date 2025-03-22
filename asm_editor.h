#pragma once
#include <vector>
#include <string>
#include "6502pasm.h"
#include "util.h"

#define SCR_W 240 // Screen width
#define SCR_H 135 // Height
#define FNT &fonts::AsciiFont8x16 // Font
#define FNT_W   8 // Font width
#define FNT_H  16 // Height
#define TXT_COLS (SCR_W/FNT_W)// Text columns
#define TXT_ROWS (SCR_H/FNT_H)// Text rows

extern Assembler assembler;
struct EditorState {
  std::vector<std::string> code = {""};
  int cursorX;
  int cursorY;
  int scrollX;
  int scrollY;
};

EditorState editor;

void editorDraw();
void editorUpdate(bool updateKB);