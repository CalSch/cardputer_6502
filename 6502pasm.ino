#include <stdio.h>
#include <string>
#include <vector>
#include <regex>
#include "6502pasm.h"


std::regex labelPattern("[a-zA-Z0-9_]+");
std::regex labelLinePattern("([a-zA-Z0-9_]+):\\s*");
std::regex instructionPattern("^\\s*([a-zA-Z]+)( (([^,\\s]*)(,(\\w))?))?\\s*$");
std::regex directivePattern("\\s*\\.(\\w+)(\\s+(.+))?");

std::regex immediatePattern("#\\$[0-9a-fA-F]{2}"); // #$BB (BB=byte)
std::regex zeropagePattern("\\$[0-9a-fA-F]{2}");   //  $LL (LL=low byte of addr)
std::regex absolutePattern("\\$[0-9a-fA-F]{4}");   //  $HHLL (HHLL = full addr)
// std::regex indirectPattern("\\(\\$[0-9a-fA-F]{4}\\)");


void Assembler::insertOutputBytes(std::vector<unsigned char> bytes, int location) {
    int endIndex = location + bytes.size() - 1;
    while (endIndex>=output_bytes.size()) {
        output_bytes.push_back(0);
    }
    for (int i=0;i<bytes.size();i++) {
        output_bytes[location+i]=bytes[i];
    }
}

int Assembler::assemble() {
    for (int pass=1;pass<=2;pass++) { // do 2 passes
        if (DEBUG) printf("** Starting pass %d **\n",pass);
        int pc = 0; // keeps track of the current position in the binary/
        for (int lineIdx=0;lineIdx<input_lines.size();lineIdx++) {
            std::string line = input_lines[lineIdx];
            if (DEBUG) printf("Reading line: '%s'\n",line.c_str());
            bool hasComment = false;
            int commentIndex = -1;
            for (int i=0;i<line.size();i++) { // find a semicolon
                if (line[i]==';') {
                    hasComment = true;
                    commentIndex = i;
                    break;
                }
            }
            std::string lineNoComment = hasComment ? line.substr(0,commentIndex) : line;
            LineType lineType = getLineType(lineNoComment);
            if (lineType == LINE_UNKNOWN) {
                printf("Unknown line: '%s'\n",line.c_str());
            }
            if (lineType == LINE_LABEL) {
                if (pass==1) {
                    Label label = parseLabel(lineNoComment);
                    label.location = pc;
                    labels.push_back(label);
                }
            }
            if (lineType == LINE_INSTRUCTION) {
                Instruction inst = parseInstruction(lineNoComment);
                AssembledInstruction asmInst = assembleInstruction(inst,pc);
                if (DEBUG) {
                    // printf("Instruction: '%s '")
                    printf("Assembled: ");
                    for (unsigned char byte : asmInst.bytes) {
                        printf("%02x ",byte);
                    }
                    printf("\n");
                }
                if (pass==2) insertOutputBytes(asmInst.bytes,pc);
                pc += asmInst.bytes.size();
            }
            if (lineType == LINE_DIRECTIVE) {
                // TODO: maybe put this into a parseDirective function
                std::smatch m;
                std::regex_match(lineNoComment,m,directivePattern);
                if (DEBUG) {
                    printf("Parsing directive: '%s'\n",lineNoComment.c_str());
                    int i=0;
                    for (auto group : m) {
                        printf("\tgroup %d: '%s'\n",i,group.str().c_str());
                        i++;
                    }
                }
                std::string name = m[1].str();
                // convert to lowercase
                std::transform(name.begin(), name.end(), name.begin(),
                    [](unsigned char c){ return std::tolower(c); });
                std::vector<std::string> args;
                string_split(m[3].str(),args,' ');
                if (DEBUG) {
                    int i=0;
                    for (auto arg : args) {
                        printf("\tArgument %d: '%s'\n",i,arg.c_str());
                        i++;
                    }
                }
                if (name=="org") {
                    if (args.size()>=1) {
                        // printf("'%s'\n",args[0].substr(1));
                        pc = std::stoi(args[0].substr(1),0,16);
                    }
                } else if (name=="macro" && pass==1) { // Only parse macros on the first pass
                    if (args.size()==2) {
                        Macro macro = (Macro){args[0],args[1]};
                        macros.push_back(macro);
                        if (DEBUG) printf("\tAdded macro %%%s = %s\n",macro.name.c_str(),macro.text.c_str());
                    }
                }
            }
            if (DEBUG) printf("PC=%d\n",pc);
        }
    }
    return 0;
}

LineType Assembler::getLineType(std::string line) {
    if (line.size()==0) {
        return LINE_EMPTY;
    }
    if (std::regex_match(line,directivePattern)) {
        return LINE_DIRECTIVE;
    }
    if (std::regex_match(line,instructionPattern)) {
        return LINE_INSTRUCTION;
    }
    if (std::regex_match(line,labelLinePattern)) {
        return LINE_LABEL;
    }
    return LINE_UNKNOWN;
}

Label Assembler::parseLabel(std::string line) {
    std::smatch m;
    std::regex_match(line,m,labelLinePattern);

    if (DEBUG) {
        printf("Parsing label: '%s'\n",line.c_str());
        int i=0;
        for (auto group : m) {
            printf("\tgroup %d: '%s'\n",i,group.str().c_str());
            i++;
        }
    }

    std::string name = m[1];
    return (Label){name,-1}; // location is unknown for now
}

Address Assembler::parseAddress(std::string str) {
    if (str[0] == '%') { // it's a macro!
        std::string macroName = str.substr(1); // get all characters after %
        // Find the macro
        Macro macro;
        bool found = false;
        for (int i=0;i<macros.size();i++) {
            if (macros[i].name==macroName) {
                macro = macros[i];
                found = true;
                break;
            }
        }
        if (!found) {
            //TODO: error
        } else {
            str = macro.text; // Replace text and continue
        }
    }
    if (str[0] == '#' && str[1] == '$' && str.size() == 4) { // Immediate (#$BB)
        printf("address: %s -> %s = %02x\n",str.c_str(),str.substr(2,2).c_str(),stoi(str.substr(2,2),0,16));
        return (Address){
            ADDR_IMMEDIATE,
            stoi(str.substr(2,2),0,16) // convert hex str to int
        };
    }
    if (str[0] == '$') {
        if (str.size()==3) { // zeropage or relative, no index ($LL or $BB)
            return (Address){
                ADDR_ZERO_PAGE, // just returns zeropage, but later it will be converted into relative if its a branch instruction
                stoi(str.substr(1,2),0,16) // convert hex str to int
            };
        }
        if (str.size()==5) { // Either absolute no index or zeropage with index
            // Now find which one!
            if (str[3]==',') { // it's zeropage indexed!
                if (str[4]=='X') {
                    return (Address){
                        ADDR_ZERO_PAGE_X,
                        stoi(str.substr(1,2),0,16)
                    };
                }
                if (str[4]=='Y') {
                    return (Address){
                        ADDR_ZERO_PAGE_Y,
                        stoi(str.substr(1,2),0,16)
                    };
                }
                // If there's no X or Y, keep going to return an unknown address
            }
            if (IS_HEX_CHAR(str[3])) { // it's absolute not indexed!
                return (Address){
                    ADDR_ABSOLUTE,
                    stoi(str.substr(1,4),0,16)
                };
            }
        }
        
    }
    if (std::regex_match(str,labelPattern)) { // it's a label!
        int location = -1;
        for (Label l : labels) {
            if (l.name == str) {
                location = l.location;
            }
        }
        return (Address){
            ADDR_ABSOLUTE,
            location,
            str
        };
    }
    return (Address){
        ADDR_INVALID,
        -1
    };
}

Instruction Assembler::parseInstruction(std::string line) {
    std::smatch m;
    std::regex_match(line,m,instructionPattern);

    if (DEBUG) {
        printf("Parsing instruction: '%s'\n",line.c_str());
        int i=0;
        for (auto group : m) {
            printf("\tgroup %d: '%s'\n",i,group.str().c_str());
            i++;
        }
    }

    std::string mnemonic = m[1];

    // convert mnemonic to lowercase
    std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(),
        [](unsigned char c){ return std::toupper(c); });

    Address addr;
    if (m[3]!="") {
        addr = parseAddress(m[3]);
        if (DEBUG) printf("\t(%d,%04x,%s)\n",addr.addrMode,addr.location,addr.label.c_str());
    } else {
        addr.addrMode = ADDR_IMPLIED;
    }

    Instruction inst = (Instruction){mnemonic,addr};
    return inst;
}

AssembledInstruction Assembler::assembleInstruction(Instruction inst, int pc) {
    AssembledInstruction out;
    out.orig = inst;
    for (const auto& [mnemonic, modes] : opcode_lut) {
        if (mnemonic != inst.mnemonic) {
            continue;
        }
        for (const auto& [mode, bytes] : modes) {
            if (mode != inst.addr.addrMode) {
                continue;
            }
            for (char byte : bytes) {
                // printf("Adding byte: %c %02x %02x\n",byte,byte,VALUE_L);
                if (byte==(char)VALUE_L) {
                    // printf("VALUE_L: %04x -> %02x\n",inst.addr.location,inst.addr.location&0xff);
                    out.bytes.push_back(inst.addr.location&0xff);
                } else if (byte==(char)VALUE_H) {
                    out.bytes.push_back(inst.addr.location >> 8);
                } else if (byte==(char)RELATIVE_ADDR) {
                    out.bytes.push_back((inst.addr.location-(pc+2))&0xff);
                } else {
                    out.bytes.push_back(byte & 0xff);
                }
            }
        }
    }
    return out;
}

// From https://stackoverflow.com/a/5888676
size_t string_split(const std::string &txt, std::vector<std::string> &strs, char ch)
{
    size_t pos = txt.find( ch );
    size_t initialPos = 0;
    strs.clear();

    // Decompose statement
    while( pos != std::string::npos ) {
        strs.push_back( txt.substr( initialPos, pos - initialPos ) );
        initialPos = pos + 1;

        pos = txt.find( ch, initialPos );
    }

    // Add the last one
    strs.push_back( txt.substr( initialPos, std::min( pos, txt.size() ) - initialPos + 1 ) );

    return strs.size();
}

// Opcode table taken from http://www.dabeaz.com/superboard/asm6502.py
// and translated into C++ with ChatGPT, so if it doesn't work blame them
// VALUE_L,VALUE_H, and RELATIVE_ADDR should be replaced with their respective values
std::map<std::string, std::map<AddressMode, std::vector<int>>> opcode_lut =
    {
        {"DATA", {
            {ADDR_IMMEDIATE, {VALUE_L}}
        }},
        {"ADC", {
            {ADDR_IMMEDIATE, {0x69, VALUE_L}},
            {ADDR_ZERO_PAGE, {0x65, VALUE_L}},
            {ADDR_ZERO_PAGE_X, {0x75, VALUE_L}},
            {ADDR_ABSOLUTE, {0x6D, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_X, {0x7D, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_Y, {0x79, VALUE_L, VALUE_H}},
            {ADDR_INDIRECT_X, {0x61, VALUE_L}},
            {ADDR_INDIRECT_Y, {0x71, VALUE_L}}
        }},
        {"AND", {
            {ADDR_IMMEDIATE, {0x29, VALUE_L}},
            {ADDR_ZERO_PAGE, {0x25, VALUE_L}},
            {ADDR_ZERO_PAGE_X, {0x35, VALUE_L}},
            {ADDR_ABSOLUTE, {0x2D, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_X, {0x3D, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_Y, {0x39, VALUE_L, VALUE_H}},
            {ADDR_INDIRECT_X, {0x21, VALUE_L}},
            {ADDR_INDIRECT_Y, {0x31, VALUE_L}}
        }},
        {"ASL", {
            {ADDR_ACCUMULATOR, {0x0a}},
            {ADDR_ZERO_PAGE, {0x06, VALUE_L}},
            {ADDR_ZERO_PAGE_X, {0x16, VALUE_L}},
            {ADDR_ABSOLUTE, {0x0e, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_X, {0x1e, VALUE_L, VALUE_H}}
        }},
        {"BIT", {
            {ADDR_ZERO_PAGE, {0x24, VALUE_L}},
            {ADDR_ABSOLUTE, {0x2c, VALUE_L, VALUE_H}}
        }},
        {"BPL", {
            {ADDR_IMMEDIATE, {0x10, VALUE_L}},
            {ADDR_ABSOLUTE, {0x10, RELATIVE_ADDR}}
        }},
        {"BMI", {
            {ADDR_IMMEDIATE, {0x30, VALUE_L}},
            {ADDR_ABSOLUTE, {0x30, RELATIVE_ADDR}}
        }},
        {"BVC", {
            {ADDR_IMMEDIATE, {0x50, VALUE_L}},
            {ADDR_ABSOLUTE, {0x50, RELATIVE_ADDR}}
        }},
        {"BVS", {
            {ADDR_IMMEDIATE, {0x70, VALUE_L}},
            {ADDR_ABSOLUTE, {0x70, RELATIVE_ADDR}}
        }},
        {"BCC", {
            {ADDR_IMMEDIATE, {0x90, VALUE_L}},
            {ADDR_ABSOLUTE, {0x90, RELATIVE_ADDR}}
        }},
        {"BCS", {
            {ADDR_IMMEDIATE, {0xb0, VALUE_L}},
            {ADDR_ABSOLUTE, {0xb0, RELATIVE_ADDR}}
        }},
        {"BNE", {
            {ADDR_IMMEDIATE, {0xd0, VALUE_L}},
            {ADDR_ABSOLUTE, {0xd0, RELATIVE_ADDR}}
        }},
        {"BEQ", {
            {ADDR_IMMEDIATE, {0xf0, VALUE_L}},
            {ADDR_ABSOLUTE, {0xf0, RELATIVE_ADDR}}
        }},
        {"BRK", {
            {ADDR_IMPLIED, {0x00}},
        }},
        {"CMP", {
            {ADDR_IMMEDIATE, {0xc9, VALUE_L}},
            {ADDR_ZERO_PAGE, {0xc5, VALUE_L}},
            {ADDR_ZERO_PAGE_X, {0xd5, VALUE_L}},
            {ADDR_ABSOLUTE, {0xcD, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_X, {0xdD, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_Y, {0xd9, VALUE_L, VALUE_H}},
            {ADDR_INDIRECT_X, {0xc1, VALUE_L}},
            {ADDR_INDIRECT_Y, {0xd1, VALUE_L}}
        }},
        {"CPX", {
            {ADDR_IMMEDIATE, {0xe0, VALUE_L}},
            {ADDR_ZERO_PAGE, {0xe4, VALUE_L}},
            {ADDR_ABSOLUTE, {0xec, VALUE_L, VALUE_H}}
        }},
        {"CPY", {
            {ADDR_IMMEDIATE, {0xc0, VALUE_L}},
            {ADDR_ZERO_PAGE, {0xc4, VALUE_L}},
            {ADDR_ABSOLUTE, {0xcc, VALUE_L, VALUE_H}}
        }},
        {"DEC", {
            {ADDR_ZERO_PAGE, {0xc6, VALUE_L}},
            {ADDR_ZERO_PAGE_X, {0xd6, VALUE_L}},
            {ADDR_ABSOLUTE, {0xce, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_X, {0xde, VALUE_L, VALUE_H}}
        }},
        {"EOR", {
            {ADDR_IMMEDIATE, {0x49, VALUE_L}},
            {ADDR_ZERO_PAGE, {0x45, VALUE_L}},
            {ADDR_ZERO_PAGE_X, {0x55, VALUE_L}},
            {ADDR_ABSOLUTE, {0x4D, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_X, {0x5D, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_Y, {0x59, VALUE_L, VALUE_H}},
            {ADDR_INDIRECT_X, {0x41, VALUE_L}},
            {ADDR_INDIRECT_Y, {0x51, VALUE_L}}
        }},
        {"CLC", {
            {ADDR_IMPLIED, {0x18}}
        }},
        {"SEC", {
            {ADDR_IMPLIED, {0x38}}
        }},
        {"CLI", {
            {ADDR_IMPLIED, {0x58}}
        }},
        {"SEI", {
            {ADDR_IMPLIED, {0x78}}
        }},
        {"CLV", {
            {ADDR_IMPLIED, {0xb8}}
        }},
        {"CLD", {
            {ADDR_IMPLIED, {0xd8}}
        }},
        {"SED", {
            {ADDR_IMPLIED, {0xf8}}
        }},
        {"INC", {
            {ADDR_ZERO_PAGE, {0xe6, VALUE_L}},
            {ADDR_ZERO_PAGE_X, {0xf6, VALUE_L}},
            {ADDR_ABSOLUTE, {0xee, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_X, {0xfe, VALUE_L, VALUE_H}}
        }},
        {"JMP", {
            {ADDR_ABSOLUTE, {0x4c, VALUE_L, VALUE_H}},
            {ADDR_INDIRECT, {0x6c, VALUE_L, VALUE_H}}
        }},
        {"JSR", {
            {ADDR_ABSOLUTE, {0x20, VALUE_L, VALUE_H}}
        }},
        {"LDA", {
            {ADDR_IMMEDIATE, {0xA9, VALUE_L}},
            {ADDR_ZERO_PAGE, {0xA5, VALUE_L}},
            {ADDR_ZERO_PAGE_X, {0xB5, VALUE_L}},
            {ADDR_ABSOLUTE, {0xAD, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_X, {0xBD, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_Y, {0xB9, VALUE_L, VALUE_H}},
            {ADDR_INDIRECT_X, {0xA1, VALUE_L}},
            {ADDR_INDIRECT_Y, {0xB1, VALUE_L}}
        }},
        {"LDX", {
            {ADDR_IMMEDIATE, {0xa2, VALUE_L}},
            {ADDR_ZERO_PAGE, {0xa6, VALUE_L}},
            {ADDR_ZERO_PAGE_Y, {0xb6, VALUE_L}},
            {ADDR_ABSOLUTE, {0xae, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_Y, {0xbe, VALUE_L, VALUE_H}}
        }},
        {"LDY", {
            {ADDR_IMMEDIATE, {0xa0, VALUE_L}},
            {ADDR_ZERO_PAGE, {0xa4, VALUE_L}},
            {ADDR_ZERO_PAGE_X, {0xb4, VALUE_L}},
            {ADDR_ABSOLUTE, {0xac, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_X, {0xbc, VALUE_L, VALUE_H}}
        }},
        {"LSR", {
            {ADDR_ACCUMULATOR, {0x4a}},
            {ADDR_ZERO_PAGE, {0x46, VALUE_L}},
            {ADDR_ZERO_PAGE_X, {0x56, VALUE_L}},
            {ADDR_ABSOLUTE, {0x4e, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_X, {0x5e, VALUE_L, VALUE_H}}
        }},
        {"NOP", {
            {ADDR_IMPLIED, {0xea}}
        }},
        {"ORA", {
            {ADDR_IMMEDIATE, {0x09, VALUE_L}},
            {ADDR_ZERO_PAGE, {0x05, VALUE_L}},
            {ADDR_ZERO_PAGE_X, {0x15, VALUE_L}},
            {ADDR_ABSOLUTE, {0x0D, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_X, {0x1D, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_Y, {0x19, VALUE_L, VALUE_H}},
            {ADDR_INDIRECT_X, {0x01, VALUE_L}},
            {ADDR_INDIRECT_Y, {0x11, VALUE_L}}
        }},
        {"TAX", {
            {ADDR_IMPLIED, {0xaa}}
        }},
        {"TXA", {
            {ADDR_IMPLIED, {0x8a}}
        }},
        {"DEX", {
            {ADDR_IMPLIED, {0xca}}
        }},
        {"INX", {
            {ADDR_IMPLIED, {0xe8}}
        }},
        {"TAY", {
            {ADDR_IMPLIED, {0xa8}}
        }},
        {"TYA", {
            {ADDR_IMPLIED, {0x98}}
        }},
        {"DEY", {
            {ADDR_IMPLIED, {0x88}}
        }},
        {"INY", {
            {ADDR_IMPLIED, {0xc8}}
        }},
        {"ROL", {
            {ADDR_ACCUMULATOR, {0x2a}},
            {ADDR_ZERO_PAGE, {0x26, VALUE_L}},
            {ADDR_ZERO_PAGE_X, {0x36, VALUE_L}},
            {ADDR_ABSOLUTE, {0x2e, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_X, {0x3e, VALUE_L, VALUE_H}}
        }},
        {"ROR", {
            {ADDR_ACCUMULATOR, {0x6a}},
            {ADDR_ZERO_PAGE, {0x66, VALUE_L}},
            {ADDR_ZERO_PAGE_X, {0x76, VALUE_L}},
            {ADDR_ABSOLUTE, {0x6e, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_X, {0x7e, VALUE_L, VALUE_H}}
        }},
        {"RTI", {
            {ADDR_IMPLIED, {0x40}}
        }},
        {"RTS", {
            {ADDR_IMPLIED, {0x60}}
        }},
        {"SBC", {
            {ADDR_IMMEDIATE, {0xe9, VALUE_L}},
            {ADDR_ZERO_PAGE, {0xe5, VALUE_L}},
            {ADDR_ZERO_PAGE_X, {0xf5, VALUE_L}},
            {ADDR_ABSOLUTE, {0xeD, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_X, {0xfD, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_Y, {0xf9, VALUE_L, VALUE_H}},
            {ADDR_INDIRECT_X, {0xe1, VALUE_L}},
            {ADDR_INDIRECT_Y, {0xf1, VALUE_L}}
        }},
        {"STA", {
            {ADDR_ZERO_PAGE, {0x85, VALUE_L}},
            {ADDR_ZERO_PAGE_X, {0x95, VALUE_L}},
            {ADDR_ABSOLUTE, {0x8D, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_X, {0x9D, VALUE_L, VALUE_H}},
            {ADDR_ABSOLUTE_Y, {0x99, VALUE_L, VALUE_H}},
            {ADDR_INDIRECT_X, {0x81, VALUE_L}},
            {ADDR_INDIRECT_Y, {0x91, VALUE_L}}
        }},
        {"TXS", {
            {ADDR_IMPLIED, {0x9a}}
        }},
        {"TSX", {
            {ADDR_IMPLIED, {0xba}}
        }},
        {"PHA", {
            {ADDR_IMPLIED, {0x48}}
        }},
        {"PLA", {
            {ADDR_IMPLIED, {0x68}}
        }},
        {"PHP", {
            {ADDR_IMPLIED, {0x08}}
        }},
        {"PLP", {
            {ADDR_IMPLIED, {0x28}}
        }},
        {"STX", {
            {ADDR_ZERO_PAGE, {0x86, VALUE_L}},
            {ADDR_ZERO_PAGE_Y, {0x96, VALUE_L}},
            {ADDR_ABSOLUTE, {0x8e, VALUE_L, VALUE_H}}
        }},
        {"STY", {
            {ADDR_ZERO_PAGE, {0x84, VALUE_L}},
            {ADDR_ZERO_PAGE_X, {0x94, VALUE_L}},
            {ADDR_ABSOLUTE, {0x8c, VALUE_L, VALUE_H}}
        }}
    };