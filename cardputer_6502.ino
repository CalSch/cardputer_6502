#include "M5Cardputer.h"
#include "MCS6502.h"
#include "6502pasm.h"
#include "asm_editor.h"
#include <vector>
#include <stdio.h>
#include "keys.h"
#include "util.h"

#define MEMORY_SIZE 0x10000 // 64kib

#define SCR_W 240 // Screen width
#define SCR_H 135 // Height
#define FNT &fonts::AsciiFont8x16 // Font
#define FNT_W   8 // Font width
#define FNT_H  16 // Height
#define TXT_COLS (SCR_W/FNT_W)// Text columns
#define TXT_ROWS (SCR_H/FNT_H)// Text rows

#define MAX(a,b) (a>b?a:b)
#define MIN(a,b) (a>b?a:b)

#define BIT(n) (1<<(n-1))

enum EmuRunMode {
  RUN_MODE_STEP,
  RUN_MODE_AUTO
};
char *EmuRunModeText[] = {
  "STEP",
  "RUN"
};
struct EmuState {
  EmuRunMode runMode = RUN_MODE_STEP;
  int clockSpeed = 100; // in Hz
};

enum InterfaceState {
  INTERFACE_EMU,
  INTERFACE_EDITOR
};

InterfaceState interfaceState;
EmuState emuState;
MCS6502ExecutionContext context;
Assembler assembler;
uint8 memory[MEMORY_SIZE];

int selection = -1;

uint64_t lastMicros;
uint64_t lastScreenUpdateMicros;

char* format_scratch;


uint8 readBytesFunction(uint16 addr, void* readWriteContext) {
  Serial.printf("READ  $%04x -> #%02x\n",addr,memory[addr]);
  return memory[addr];
}

void writeBytesFunction(uint16 addr, uint8 byte, void * readWriteContext) {
  Serial.printf("WRITE $%04x -> #%02x\n",addr,byte);
  memory[addr] = byte;
}


void setup() {
  Serial.begin(921600);
  Serial.println("hi");

  auto cfg = M5.config();
  M5Cardputer.begin(cfg,true);

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextColor(GREEN);
  // M5Cardputer.Display.setTextDatum(left);
  M5Cardputer.Display.setTextFont(FNT);
  M5Cardputer.Display.setTextSize(1);
  
  MCS6502Init(&context, readBytesFunction, writeBytesFunction, NULL);  // Final param is optional context.

  emuHardReset();

  M5Cardputer.Display.drawString("hello",0,0);

  Serial.println("writing text...");

  assembler.input_lines.push_back(std::string(".ORG $0800"));
  assembler.input_lines.push_back(std::string("main:"));
  assembler.input_lines.push_back(std::string(" lda #$00"));
  assembler.input_lines.push_back(std::string(" ldx #$05"));
  assembler.input_lines.push_back(std::string("loop:"));
  assembler.input_lines.push_back(std::string(" adc #$03"));
  assembler.input_lines.push_back(std::string(" dex"));
  assembler.input_lines.push_back(std::string(" bne loop"));
  assembler.input_lines.push_back(std::string("end:"));
  assembler.input_lines.push_back(std::string(" jmp end"));

  editor.code = assembler.input_lines;
  
  emuAssembleAndLoad();

  Serial.println("Done");
  Serial.println(assembler.output_bytes.size());
  // Serial.println(assembler.output_bytes.c_str());
  // Serial.println("---------");
  // Serial.println(list_text.size());
  // Serial.println(list_text.c_str());

  updateScreen();
}

void emuHardReset() {
  for (int i=0;i<MEMORY_SIZE;i++)
    memory[i] = 0;
  
  MCS6502Reset(&context);
  context.pc=0x800;
}

void emuAssembleAndLoad() {
  Serial.println("assembling...");

  assembler.assemble();

  Serial.println("writing to memory...");

  for (int i=0;i<assembler.output_bytes.size();i++) {
    // printf("  mem[%04x] = %02x\n",i,i,assembler.output_bytes[i]);
    memory[i]=assembler.output_bytes[i];
  }
}

void drawTextWithSelection(int selectIdx, char* str, int x, int y, int color = TFT_GREEN) {
  M5Cardputer.Display.setTextColor(selection == selectIdx ? TFT_BLACK : color);
  if (selection == selectIdx) M5Cardputer.Display.fillRect(x,y,FNT_W*strlen(str),FNT_H,color);
  M5Cardputer.Display.drawString(str,x,y);
  M5Cardputer.Display.setTextColor(color);
}

// Gets the dissassembled instruction at the current Program Counter
char* getCurrentInst() {
  byte opcode = memory[context.pc];
  MCS6502Instruction* instruction = MCS6502OpcodeTable[opcode];
  char * dis = DisassembleCurrentInstruction(instruction, &context);
  return dis;
}

void showRAMImage() {
  for (int i=0;i<MEMORY_SIZE;i++) {
    char r = memory[i+0];
    if ((i)<135*240)
    M5Cardputer.Display.drawPixel(
      (i)%240,
      (i)/240,
      r
    );
  }
}

void drawCPUState(int ox, int oy) {
  drawTextWithSelection(0,format_text("PC=%04x",context.pc),ox+0,oy+FNT_H*0);
  drawTextWithSelection(1,format_text("A=%02x,%03d",context.a,context.a),  ox+0,oy+FNT_H*1);
  drawTextWithSelection(2,format_text("X=%02x,%03d",context.x,context.x),  ox+0,oy+FNT_H*2);
  drawTextWithSelection(3,format_text("Y=%02x,%03d",context.y,context.y),  ox+0,oy+FNT_H*3);
  M5Cardputer.Display.drawString(format_text("SP=%02x",context.sp),ox+0,oy+FNT_H*4);
  M5Cardputer.Display.drawString(format_text("%c%c%c%c%c%c%c%c",
    context.p & BIT(7) ? 'N' : 'n',
    context.p & BIT(6) ? 'V' : 'v',
    context.p & BIT(5) ? '-' : '-',
    context.p & BIT(4) ? 'B' : 'b',
    context.p & BIT(3) ? 'D' : 'd',
    context.p & BIT(2) ? 'I' : 'i',
    context.p & BIT(1) ? 'Z' : 'z',
    context.p & BIT(0) ? 'C' : 'c'
  ),ox+0,oy+FNT_H*5);
  M5Cardputer.Display.drawString(getCurrentInst(),ox+0,oy+FNT_H*6);
}

void drawEmuState(int ox, int oy) {
  M5Cardputer.Display.drawString(format_text("%s", EmuRunModeText[emuState.runMode]), ox+0, oy+0);
  M5Cardputer.Display.drawString(format_text("%04dHz",emuState.clockSpeed), ox+SCR_W-6*FNT_W, oy+0); // Justify right
}

void updateScreen() {
  M5Cardputer.Display.clearDisplay();
  // M5Cardputer.Display.drawString(getStatusString(),0,0);
  // M5Cardputer.Display.drawString(format_text("test %x",context.pc),0,0);
  drawEmuState(0,0);
  M5Cardputer.Display.drawFastHLine(0,FNT_H-1,SCR_W,TFT_WHITE);
  drawCPUState(0,FNT_H*1);
  // showRAMImage();
}

void emuTick() {
  // MCS6502Tick(&context); // Does one clock cycle
  MCS6502ExecNext(&context); // Runs until the end of the instruction
  if (micros()-lastScreenUpdateMicros>(1000000/10)) { // if it's been 1/20th of a second since last update...
    lastScreenUpdateMicros = micros();
    updateScreen();
  }
}

void onKeyPress(char c) {
  Serial.printf("key: %c\n",c);
  if (c>='0' && c<='9') {
    context.x=c-'0';
    updateScreen();
  }
  switch (c) {
    case KEY_STEP:
      emuTick();
      updateScreen();
      break;
    case KEY_TOGGLE_RUN_MODE:
      if (emuState.runMode == RUN_MODE_STEP) emuState.runMode = RUN_MODE_AUTO;
      else if (emuState.runMode == RUN_MODE_AUTO) emuState.runMode = RUN_MODE_STEP;
      break;
    case '[':
      emuState.clockSpeed -= 10;
      updateScreen();
      break;
    case ']':
      emuState.clockSpeed += 10;
      updateScreen();
      break;
    case ';': // up arrow
      selection--;
      selection = MAX(selection,-1); // min value is -1
      updateScreen();
      break;
    case '.': // down arrow
      selection++;
      // TODO add a maximum value
      updateScreen();
      break;
  }
}

void onCtrlKeyPress(char c) {
  switch (c) {
    case 'A':
      emuHardReset();
      emuAssembleAndLoad();
      updateScreen();
  }
}

int vectorFind(std::vector<char> v, char c) {
  for (int i=0;i<v.size();i++) {
    if (v[i]==c)
      return i;
  }
  return -1;
}
std::vector<char> lastKeysPressed;
void loop() {
  M5Cardputer.update();

  // Serial.println("loop");
  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
  bool updateKB=false;
  if (M5Cardputer.Keyboard.isChange()) {
    if (M5Cardputer.Keyboard.isPressed()) {
      updateKB=true;
      if (status.tab) {
        interfaceState = (interfaceState==INTERFACE_EMU ? INTERFACE_EDITOR : INTERFACE_EMU);
        if (interfaceState==INTERFACE_EMU)
          updateScreen();
      }
    }
  }
  if (interfaceState == INTERFACE_EDITOR) {
    editorUpdate(updateKB);
    return;
  }
  // Serial.println("hi");
  if (updateKB) {
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
    
    for (char c : status.word) {
      int idx = vectorFind(lastKeysPressed, tolower(c));
      if (idx==-1) {
        if (status.ctrl)
          onCtrlKeyPress(c);
        // else if (status.fn)
        //   onFnKeyPress(c);
        // else if (status.opt)
        //   onOptKeyPress(c);
        else
          onKeyPress(c);
        lastKeysPressed.push_back(c);
      }
    }
    if (status.enter) {
      // onEnterPress();
    }
    if (status.del) {
      // onDeletePress();
    }
    // updateScreen();
  }
  for (int i=0;i<lastKeysPressed.size();i++) {
    if (!M5Cardputer.Keyboard.isKeyPressed(lastKeysPressed[i])) {
      lastKeysPressed.erase(lastKeysPressed.begin()+i);
    }
  }
  // put your main code here, to run repeatedly
  // M5Cardputer.Display.clearDisplay(RED);
  if (emuState.runMode == RUN_MODE_AUTO) {
    emuTick();
  }
  // M5Cardputer.Display.drawString(,0,16);
  // M5Cardputer.Display.drawString(,0,32);
  // showRAMImage();
  lastMicros = micros();
  delayMicroseconds(1000000/emuState.clockSpeed);
}
