#include "M5Cardputer.h"
#include "MCS6502.h"
#include <vector>
#include "keys.h"

#define MEMORY_SIZE 0x10000 // 64kib

#define SCR_W 240 // Screen width
#define SCR_H 135 // Height
#define FNT &fonts::AsciiFont8x16 // Font
#define FNT_W   8 // Font width
#define FNT_H  16 // Height
#define TXT_COLS (SCR_W/FNT_W)// Text columns
#define TXT_ROWS (SCR_H/FNT_H)// Text rows

#define BIT(n) (1<<(n-1))

enum EmuRunMode {
  RUN_MODE_STEP,
  RUN_MODE_AUTO
};
struct EmuState {
  EmuRunMode runMode = RUN_MODE_STEP;
  int clockSpeed = 100; // in Hz
};

EmuState currentState;
MCS6502ExecutionContext context;
uint8 memory[MEMORY_SIZE];
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

char* format_text(const char *format, ...) {
    free(format_scratch);

    va_list args;

    va_start(args, format);
    if(0 > vasprintf(&format_scratch, format, args)) format_scratch = NULL;    //this is for logging, so failed allocation is not fatal
    va_end(args);

    if(format_scratch) {
      return format_scratch;
    } else {
      return "uh oh";
    }
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

  for (int i=0;i<MEMORY_SIZE;i++)
    memory[i] = 0;


  M5Cardputer.Display.drawString("hello",0,0);

  // set reset vector to 0x800
  memory[0xfffc] = 0x00;
  memory[0xfffd] = 0x08;

  memory[0x800] = 0x69;
  memory[0x801] = 0x01;
  memory[0x802] = 0x4C;
  memory[0x803] = 0x00;
  memory[0x804] = 0x08;

  MCS6502Reset(&context);
}

char* getStatusString() {
  char* str = (char*)malloc(100);
  
  char cool_str[6]; // 5 byte test string at 0x100
  for (int i=0;i<5;i++)
    cool_str[i]=memory[0x100+i];
  
  byte opcode = memory[context.pc];
  MCS6502Instruction* instruction = MCS6502OpcodeTable[opcode];
  char * dis = DisassembleCurrentInstruction(instruction, &context);

  sprintf(str,"PC=%04x A=%02x %s",context.pc,context.a,dis);
  return str;
}

void showRAMImage() {
  for (int i=0;i<MEMORY_SIZE;i++) {
    char r = memory[i+0];
    if ((i)/240<135)
    M5Cardputer.Display.drawPixel(
      (i)%240,
      (i)/240,
      r
    );
  }
}

void drawCPUState(int ox, int oy) {
  M5Cardputer.Display.drawString(format_text("PC=%04x",context.pc),ox+0,oy+FNT_H*0);
  M5Cardputer.Display.drawString(format_text("A=%02x",context.a),  ox+0,oy+FNT_H*1);
  M5Cardputer.Display.drawString(format_text("X=%02x",context.x),  ox+0,oy+FNT_H*2);
  M5Cardputer.Display.drawString(format_text("Y=%02x",context.y),  ox+0,oy+FNT_H*3);
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
}

void updateScreen() {
  M5Cardputer.Display.clearDisplay();
  // M5Cardputer.Display.drawString(getStatusString(),0,0);
  // M5Cardputer.Display.drawString(format_text("test %x",context.pc),0,0);
  drawCPUState(0,0);
  M5Cardputer.Display.drawString(format_text("%04dHz",currentState.clockSpeed),SCR_W-6*FNT_W,SCR_H-FNT_H);
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
  switch (c) {
    case KEY_STEP:
      emuTick();
      updateScreen();
      break;
    case KEY_TOGGLE_RUN_MODE:
      if (currentState.runMode == RUN_MODE_STEP) currentState.runMode = RUN_MODE_AUTO;
      else if (currentState.runMode == RUN_MODE_AUTO) currentState.runMode = RUN_MODE_STEP;
      break;
    case '[':
      currentState.clockSpeed -= 10;
      break;
    case ']':
      currentState.clockSpeed += 10;
      break;
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
  if (M5Cardputer.Keyboard.isChange()) {
    if (M5Cardputer.Keyboard.isPressed()) {
      Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
      
      for (char c : status.word) {
        int idx = vectorFind(lastKeysPressed, tolower(c));
        if (idx==-1) {
          // if (status.ctrl)
          //   onCtrlKeyPress(c);
          // else if (status.fn)
          //   onFnKeyPress(c);
          // else if (status.opt)
          //   onOptKeyPress(c);
          // else
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
  }
  // put your main code here, to run repeatedly
  // M5Cardputer.Display.clearDisplay(RED);
  if (currentState.runMode == RUN_MODE_AUTO) {
    emuTick();
  }
  // M5Cardputer.Display.drawString(,0,16);
  // M5Cardputer.Display.drawString(,0,32);
  // showRAMImage();
  lastMicros = micros();
  delayMicroseconds(1000000/currentState.clockSpeed);
}
