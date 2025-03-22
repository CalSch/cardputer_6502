#pragma once
#include <string>
#include <vector>
#include <regex>

#define DEBUG 1

#define IS_HEX_CHAR(c) ((c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F'))

// patterns
extern std::regex labelPattern;
extern std::regex labelLinePattern;
extern std::regex instructionPattern;
extern std::regex directivePattern;
extern std::regex macroPattern;

extern std::regex immediatePattern;
extern std::regex zeropagePattern;
extern std::regex absolutePattern;
extern std::regex indirectPattern;

enum AddressMode {
    ADDR_INVALID, // For syntax errors
    ADDR_IMPLIED,
    ADDR_ACCUMULATOR,
    ADDR_IMMEDIATE,
    ADDR_ZERO_PAGE,
    ADDR_ZERO_PAGE_X,
    ADDR_ZERO_PAGE_Y,
    ADDR_ABSOLUTE,
    ADDR_ABSOLUTE_X,
    ADDR_ABSOLUTE_Y,
    ADDR_INDIRECT,
    ADDR_INDIRECT_X,
    ADDR_INDIRECT_Y,
};

enum LineType {
    LINE_UNKNOWN, // For syntax errors
    LINE_EMPTY,
    LINE_INSTRUCTION,
    LINE_LABEL,
    LINE_DIRECTIVE,
};

struct Macro {
    std::string name;
    std::string text;
};

struct Label {
    std::string name;
    int location;
};

struct Address {
    AddressMode addrMode;
    int location;
    std::string label;
};

struct Instruction {
    std::string mnemonic;
    Address addr;
};
struct AssembledInstruction {
    Instruction orig;
    std::vector<unsigned char> bytes;
};

class Assembler {
public:
    std::vector<std::string> input_lines;
    std::vector<unsigned char> output_bytes;

    std::vector<Label> labels;
    std::vector<Macro> macros;

    Assembler() {}

    LineType getLineType(std::string line);

    // Turns a string into an `Address`
    Address parseAddress(std::string addr);
    // Create and add a `Label` struct from a line
    // Note: implies that the `labelLinePattern` already matches the line (No error handling!)
    Label parseLabel(std::string line);
    Instruction parseInstruction(std::string line);

    AssembledInstruction assembleInstruction(Instruction inst,int pc);

    void insertOutputBytes(std::vector<unsigned char> bytes, int location);

    int assemble();
};

size_t string_split(const std::string &txt, std::vector<std::string> &strs, char ch);

#define VALUE_L -1
#define VALUE_H -2
#define RELATIVE_ADDR -3

// Opcode table taken from http://www.dabeaz.com/superboard/asm6502.py
// and translated into C++ with ChatGPT, so if it doesn't work blame them
// VALUE_L,VALUE_H, and RELATIVE_ADDR should be replaced with their respective values
extern std::map<std::string, std::map<AddressMode, std::vector<int>>> opcode_lut;
