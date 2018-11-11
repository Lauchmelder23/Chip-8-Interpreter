////////////////////////////////////////////////////////////////
// A CHIP-8 INTERPRETER, CONTAINED IN ONE FILE
//
// INTERPRETATION LOGIC DONE BY ME
//
// GRAPHICS HANDLING CODED BY JAVIDX9
// https://www.youtube.com/channel/UC-yuWVUplUJZvieEligKBkA
//
// I used the snippets I needed from his olcGameEngine.hpp
// I removed everything not needed to draw pixels to a console
//
/////////////////////////////////////////////////////////////////



#include <array>
#include <fstream>
#include <iostream>
#include <random>
#include <chrono>
#include <unordered_map>
#include <string>

#pragma comment(lib, "winmm.lib")

#ifndef UNICODE
#error Please enable UNICODE for your compiler! VS: Project Properties -> General -> \
Character Set -> Use Unicode. Thanks! - Javidx9
#endif

#include <windows.h>

#include <atomic>
#include <condition_variable>

#define SUPPRESS_PROC_INFO


enum COLOUR
{
	FG_BLACK = 0x0000,
	FG_DARK_BLUE = 0x0001,
	FG_DARK_GREEN = 0x0002,
	FG_DARK_CYAN = 0x0003,
	FG_DARK_RED = 0x0004,
	FG_DARK_MAGENTA = 0x0005,
	FG_DARK_YELLOW = 0x0006,
	FG_GREY = 0x0007, // Thanks MS :-/
	FG_DARK_GREY = 0x0008,
	FG_BLUE = 0x0009,
	FG_GREEN = 0x000A,
	FG_CYAN = 0x000B,
	FG_RED = 0x000C,
	FG_MAGENTA = 0x000D,
	FG_YELLOW = 0x000E,
	FG_WHITE = 0x000F,
	BG_BLACK = 0x0000,
	BG_DARK_BLUE = 0x0010,
	BG_DARK_GREEN = 0x0020,
	BG_DARK_CYAN = 0x0030,
	BG_DARK_RED = 0x0040,
	BG_DARK_MAGENTA = 0x0050,
	BG_DARK_YELLOW = 0x0060,
	BG_GREY = 0x0070,
	BG_DARK_GREY = 0x0080,
	BG_BLUE = 0x0090,
	BG_GREEN = 0x00A0,
	BG_CYAN = 0x00B0,
	BG_RED = 0x00C0,
	BG_MAGENTA = 0x00D0,
	BG_YELLOW = 0x00E0,
	BG_WHITE = 0x00F0,
};

enum PIXEL_TYPE
{
	PIXEL_SOLID = 0x2588,
	PIXEL_THREEQUARTERS = 0x2593,
	PIXEL_HALF = 0x2592,
	PIXEL_QUARTER = 0x2591,
};



typedef unsigned short WORD;
typedef unsigned char BYTE;

const constexpr unsigned WIDTH = 64;
const constexpr unsigned HEIGHT = 32;
const constexpr unsigned RAM = 4096; // 4kB RAM
const constexpr unsigned FONTSET_SIZE = 16 * 5;
const constexpr unsigned SCALE = 20;
const constexpr char*	 FILENAME = "invaders.c8";

BYTE fontset[FONTSET_SIZE] =
{
	0xF0, 0x90, 0x90, 0x90, 0xF0,		// 0
	0x20, 0x60, 0x20, 0x20, 0x70,		// 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0,		// 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0,		// 3
	0x90, 0x90, 0xF0, 0x10, 0x10,		// 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0,		// 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0,		// 6
	0xF0, 0x10, 0x20, 0x40, 0x40,		// 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0,		// 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0,		// 9
	0xF0, 0x90, 0xF0, 0x90, 0x90,		// A
	0xE0, 0x90, 0xE0, 0x90, 0xE0,		// B
	0xF0, 0x80, 0x80, 0x80, 0xF0,		// C
	0xE0, 0x90, 0x90, 0x90, 0xE0,		// D
	0xF0, 0x80, 0xF0, 0x80, 0xF0,		// E
	0xF0, 0x80, 0xF0, 0x80, 0x80		// F
};

std::unordered_map<BYTE, int> keymap =
{
	{ 0x1, '1'		},{ 0x2, '2'	},{ 0x3, '3'	},{ 0xC, '4'	},
	{ 0x4, 'Q'		},{ 0x5, 'W'	},{ 0x6, 'E'	},{ 0xD, 'R'	},
	{ 0x7, 'A'		},{ 0x8, 'S'	},{ 0x9, 'D'	},{ 0xE, 'F'	},
	{ 0xA, 'Y'		},{ 0x0, 'X'	},{ 0xB, 'C'	},{ 0xF, 'V'	}
};

class Chip8
{
public:

	bool interrupt;
	bool drawFlag;

	BYTE delay_timer;
	BYTE sound_timer;

	//////////////////////////////////////////////
	/// \brief Intializes registers and memory
	///
	//////////////////////////////////////////////
	void Initialize()
	{
		pc = 0x200;	// Program counter starts at 0x200
		opcode = 0;		// Reset opcode
		I = 0;		// Reset index register
		sp = 0;		// Reset stack pointer

		std::fill(std::begin(gfx), std::end(gfx), 0x00); // Clear display
		std::fill(std::begin(stack), std::end(stack), 0x00); // Clear stack
		std::fill(std::begin(memory), std::end(memory), 0x00); // Clear RAM
		std::fill(std::begin(V), std::end(V), 0x00); // Clear Registers

		for (int i = 0; i < FONTSET_SIZE; i++)				   // Load fontset
			memory[i] = fontset[i];

		interrupt = false;
		drawFlag = false;
	}


	//////////////////////////////////////////////
	/// \brief Loads a ROM into memory
	///
	/// \param filepath The path to the ROM
	//////////////////////////////////////////////
	void LoadGame(std::string filepath)
	{
		// Open File
		std::ifstream file(filepath, std::ios::binary);

		// All bytes in the file will be stored at 0x200 in memory
		int offset = 0;
		while (file.good())
		{
			memory[0x200 + offset++] = (BYTE)file.get();
		}
	}

	//////////////////////////////////////////////
	/// \brief Returns the current display
	///
	//////////////////////////////////////////////
	BYTE* getDisplay() { return gfx; }


	//////////////////////////////////////////////
	/// \brief Goes through one emulation cycle
	///
	//////////////////////////////////////////////
	void EmulateCycle()
	{
		// Fetch opcode
		opcode = (memory[pc] << 8) | memory[pc + 1];

#ifndef SUPPRESS_PROC_INFO
		std::cout << std::uppercase << std::hex << opcode << ": ";
#endif

		BYTE x = (opcode & 0x0F00) >> 8;
		BYTE y = (opcode & 0x00F0) >> 4;
		BYTE n = (opcode & 0x000F);
		BYTE kk = (opcode & 0x00FF);
		WORD nnn = (opcode & 0x0FFF);

		// Decode opcode
		// Execute opcode

		switch (opcode & 0xF000)
		{
		case 0x0000:	// Multi case opcode
			switch (opcode & 0x00FF)
			{
			case 0xE0:	// Clear display
				CLS();
				break;

			case 0xEE:	// Return from subroutine
				RET();
				break;

			default:
				std::cerr << "Unknown OpCode" << std::endl;
				interrupt = true;
				//interrupt = true;
				break;
			}
			break;


		case 0x1000:	// 1NNN: Set Program counter to NNN
			JP(nnn);
			break;


		case 0x2000:	// 2NNN: Calls subroutine at NNN
			CALL(nnn);
			break;


		case 0x3000:	// 3XKK: If VX == KK, skip next instruction
			SE(x, kk);
			break;


		case 0x4000:	// 4XKK: If VX != KK, skip next instruction
			SNE(x, kk);
			break;

		case 0x5000:
			SE_XY(x, y);
			break;


		case 0x6000:	// 6XKK: Sets VX to KK
			LD(x, kk);
			break;


		case 0x7000:	// 7XKK: Adds KK to the value of VX
			ADD(x, kk);
			break;


		case 0x8000:	// Multi case opcode
			switch (opcode & 0x000F)
			{
			case 0x0:	// 8XY0: Set VX = VY
				LD_XY(x, y);
				break;

			case 0x1:	// 8XY1: Set VX = VX | VY
				OR(x, y);
				break;


			case 0x2:	// 8XY2: VX = VX & VY
				AND(x, y);
				break;


			case 0x3:	// 8XY3: VX = VX ^ VY
				XOR(x, y);
				break;


			case 0x4:	// 8XY4: VX = VX + VY; Carry = overflow
				ADD_XY(x, y);
				break;


			case 0x5:	// 8XY5: VX = VX - VY, set VF = NOT borrow
				SUB(x, y);
				break;


			case 0x6:	// 8XY6: VX = VX >> 1. VF = VX least significant bit
				SHR(x, y);
				break;


			case 0x7:	// 8XY7: VX = VY - VX, set VF = NOT borrow
				SUBN(x, y);
				break;


			case 0xE:
				SHL(x, y);
				break;


			default:
				std::cerr << "Unknown OpCode" << std::endl;
				interrupt = true;
				//interrupt = true;
				break;
			} break;


		case 0x9000:	// 9XY0: Skip next instruction of VX != VY
			SNE_XY(x, y);
			break;


		case 0xA000:	// ANNN: Set register I to NNN
			LD(nnn);
			break;


		case 0xB000:	// BNNN: Jump to location NNN + V0
			JP_V(nnn);
			break;


		case 0xC000:	// Set VX to a random byte & kk
			RND(x, kk);
			break;


		case 0xD000:	// DXYN: Draw the N byte long sprite stored at I at position (VX, VY). Set VF = 1 if collision
			DRW(x, y, n);
			break;


		case 0xE000:	// Multi case opcode
			switch (opcode & 0x00FF)
			{
			case 0x9E:	// EX9E: Skip next instruction is key with value VX is pressed
				SKP(x);
				break;


			case 0xA1:	// EXA1: Skip next instruction is key with value VX is not pressed
				SKNP(x);
				break;


			default:
				std::cerr << "Unknown OpCode" << std::endl;
				interrupt = true;
				//interrupt = true;
				break;
			}
			break;


		case 0xF000:	// Multi case opcode
			switch (opcode & 0x00FF)
			{
			case 0x07:	// FX07: Set VX = delay_timer
				LD_X(x);
				break;


			case 0x0A:	// FX0A: Halt program until key is pressed. Save key to VX
				LD_K(x);
				break;


			case 0x15:	// FX15: Set delay_timer = VX
				LD_DT(x);
				break;


			case 0x18:	// FX18: Set sound:timer = VX
				LD_ST(x);
				break;


			case 0x1E:	// FX1E: Set I = I + VX
				ADD_I(x);
				break;


			case 0x29:	// FX_29: Set I to font according to VX
				LD_F(x);
				break;


			case 0x33:	// FX33: Store BCD representation of VX in memory locations I, I+1 and I+2
				LD_B(x);
				break;


			case 0x55:	// Fill memory starting at I with values from V0 to VX
				LD_55(x);
				break;


			case 0x65:	// FX65: Fill V0 - VX with memory values starting at I
				LD_65(x);
				break;

			default:
				std::cerr << "Unknown OpCode" << std::endl;
				interrupt = true;
				//interrupt = true;
				break;

			} break;



		default:
			std::cerr << "Unknown OpCode" << std::endl;
			interrupt = true;
			//interrupt = true;
			break;
		}

		// Update timers
	}

private:
	WORD opcode;

	BYTE memory[RAM];
	BYTE V[16];

	WORD I;
	WORD pc;

	BYTE gfx[WIDTH * HEIGHT];

	WORD stack[16];
	WORD sp;

	BYTE key[16];

private:	// Opcodes

	///////////////////0x00E0///////////////////
	/// \brief Clears display
	///
	////////////////////////////////////////////
	void CLS()
	{
		std::fill(std::begin(gfx), std::end(gfx), 0);
		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Screen cleared" << std::endl;
#endif
	}


	///////////////////0x00EE///////////////////
	/// \brief Returns from subroutine
	///
	////////////////////////////////////////////
	void RET()
	{
		pc = stack[--sp] + 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Returned from subroutine" << std::endl;
#endif
	}

	///////////////////0x1NNN///////////////////
	/// \brief Jumps to NNN
	///
	/// \param address NNN
	///
	////////////////////////////////////////////
	void JP(WORD address)
	{
		pc = address;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Jumped to 0x" << address << std::endl;
#endif
	}

	///////////////////0x2NNN///////////////////
	/// \brief Calls subroutine at NNN
	///
	/// \param address NNN
	////////////////////////////////////////////
	void CALL(WORD address)
	{
		stack[sp++] = pc;
		pc = address;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Called subroutine at 0x" << address << std::endl;
#endif
	}


	///////////////////0x3XKK///////////////////
	/// \brief Skip next instr if VX = KK
	///
	/// \param regX X
	/// \param byte KK
	///
	////////////////////////////////////////////
	void SE(BYTE regX, BYTE kk)
	{
		if (V[regX] == kk) 
		{
			pc += 0x04;

#ifndef SUPPRESS_PROC_INFO
			std::cout << "Skipped instruction because V" << (WORD)regX << " == " << (WORD)kk << std::endl;
#endif
		}
		else 
		{
			pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
			std::cout << "Didn't skip instruction because V" << (WORD)regX << " != " << (WORD)kk << std::endl;
#endif
		}

	}


	///////////////////0x4XKK///////////////////
	/// \brief Skip next instr if VX != KK
	///
	/// \param regX X
	/// \param byte KK
	///
	////////////////////////////////////////////
	void SNE(BYTE regX, BYTE kk)
	{
		if (V[regX] != kk)
		{
			pc += 0x04;

#ifndef SUPPRESS_PROC_INFO
			std::cout << "Skipped instruction because V" << (WORD)regX << " != " << (WORD)kk << std::endl;
#endif
		}
		else
		{
			pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
			std::cout << "Didn't skip instruction because V" << (WORD)regX << " == " << (WORD)kk << std::endl;
#endif
		}

	}


	///////////////////0x5XY0///////////////////
	/// \brief Skip next instr if VX == VY
	///
	/// \param regX X
	/// \param regY Y
	///
	////////////////////////////////////////////
	void SE_XY(BYTE regX, BYTE regY)
	{
		if (V[regX] == V[regY])
		{
			pc += 0x04;

#ifndef SUPPRESS_PROC_INFO
			std::cout << "Skipped instruction because V" << (WORD)regX << " != V" << (WORD)regY << std::endl;
#endif
		}
		else
		{
			pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
			std::cout << "Didn't skip instruction because V" << (WORD)regX << " == V" << (WORD)regY << std::endl;
#endif
		}

	}


	///////////////////0x6XKK///////////////////
	/// \brief Sets VX to KK
	///
	/// \param regX X
	/// \param byte KK
	////////////////////////////////////////////
	void LD(BYTE regX, BYTE byte)
	{
		V[regX] = byte;
		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Set V" << (WORD)regX << " to 0x" << (WORD)byte << std::endl;
#endif
	}


	//////////////////0x7XKK///////////////////
	/// \brief Adds KK to VX
	///
	/// \param regX X
	/// \param byte KK
	////////////////////////////////////////////
	void ADD(BYTE regX, BYTE byte)
	{
		V[regX] += byte;
		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Added 0x" << (WORD)byte << " to V" << (WORD)regX << std::endl;
#endif
	}


	//////////////////0x8XY0///////////////////
	/// \brief Stores value of VY in VX
	///
	/// \param regX X
	/// \param regY Y
	////////////////////////////////////////////
	void LD_XY(BYTE regX, BYTE regY)
	{
		V[regX] = V[regY];

		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "V" << (WORD)regY << "(0x" << (WORD) V[regY] << ") => V" << (WORD)regX << std::endl;
#endif
	}


	//////////////////0x8XY1///////////////////
	/// \brief VX = VX | VY
	///
	/// \param regX X
	/// \param regY Y
	////////////////////////////////////////////
	void OR(BYTE regX, BYTE regY)
	{
		V[regX] |= V[regY];

		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "V" << (WORD)regX << " |= V" << (WORD)regY << "(0x" << (WORD)V[regY] << ") => " << (WORD)V[regX] << std::endl;
#endif
	}


	//////////////////0x8XY2///////////////////
	/// \brief VX = VX & VY
	///
	/// \param regX X
	/// \param regY Y
	////////////////////////////////////////////
	void AND(BYTE regX, BYTE regY)
	{
		V[regX] &= V[regY];

		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "V" << (WORD)regX << " &= V" << (WORD)regY << "(0x" << (WORD)V[regY] << ") => " << (WORD)V[regX] << std::endl;
#endif
	}


	//////////////////0x8XY3///////////////////
	/// \brief VX = VX ^ VY
	///
	/// \param regX X
	/// \param regY Y
	////////////////////////////////////////////
	void XOR(BYTE regX, BYTE regY)
	{
		V[regX] ^= V[regY];

		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "V" << (WORD)regX << " ^= V" << (WORD)regY << "(0x" << (WORD)V[regY] << ") => " << (WORD)V[regX] << std::endl;
#endif
	}


	//////////////////0x8XY4///////////////////
	/// \brief VX = VX + VY, VF = carry
	///
	/// \param regX X
	/// \param regY Y
	////////////////////////////////////////////
	void ADD_XY(BYTE regX, BYTE regY)
	{
		if (V[regY] > 0xFF - V[regX])
			V[0xF] = 1;
		else
			V[0xF] = 0;

		V[regX] = (V[regX] + V[regY]) & 0xFF;
		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "V" << (WORD)regX << " += V" << (WORD)regY << "(0x" << (WORD)V[regY] << ") => " << (WORD)V[regX] << ", Carry = " << (WORD)V[0xF] << std::endl;
#endif
	}


	//////////////////0x8XY5///////////////////
	/// \brief VX = VX - VY, VF = NOT borrow
	///
	/// \param regX X
	/// \param regY Y
	////////////////////////////////////////////
	void SUB(BYTE regX, BYTE regY)
	{
		if (V[regX] > V[regY])
			V[0xF] = 1;
		else
			V[0xF] = 0;

		V[regX] = (V[regX] - V[regY]) & 0xFF;
		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "V" << (WORD)regX << " -= V" << (WORD)regY << "(0x" << (WORD)V[regY] << ") => " << (WORD)V[regX] << ", Carry = " << (WORD)V[0xF] << std::endl;
#endif
	}


	//////////////////0x8XY6///////////////////
	/// \brief VX = VX >> 1, VF = LSB of VX
	///
	/// \param regX X
	/// \param regY Y
	////////////////////////////////////////////
	void SHR(BYTE regX, BYTE regY)
	{
		V[0xF] = V[regX] & 0x1;
		V[regX] >>= 1;

		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Shifted V" << (WORD)regX << " right once. VF = " << (WORD)V[0xF] << std::endl;
#endif
	}


	//////////////////0x8XY7///////////////////
	/// \brief VX = VY - VX, VF = NOT borrow
	///
	/// \param regX X
	/// \param regY Y
	////////////////////////////////////////////
	void SUBN(BYTE regX, BYTE regY)
	{
		if (V[regY] > V[regX])
			V[0xF] = 1;
		else
			V[0xF] = 0;

		V[regX] = (V[regY] - V[regX]) & 0xFF;
		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "V" << (WORD)regX << " = V" << (WORD)regY << "(0x" << (WORD)V[regY] << ") - V" << (WORD)regX << " => " << (WORD)V[regX] << ", Carry = " << (WORD)V[0xF] << std::endl;
#endif
	}


	//////////////////0x8XYE///////////////////
	/// \brief VX = VX >> 1, VF = LSB of VX
	///
	/// \param regX X
	/// \param regY Y
	////////////////////////////////////////////
	void SHL(BYTE regX, BYTE regY)
	{
		V[0xF] = V[regX] & 0x80;
		V[regX] <<= 1;

		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Shifted V" << (WORD)regX << " left once. VF = " << (WORD)V[0xF] << std::endl;
#endif
	}


	///////////////////0x9XY0///////////////////
	/// \brief Skip next instr if VX != VY
	///
	/// \param regX X
	/// \param regY Y
	///
	////////////////////////////////////////////
	void SNE_XY(BYTE regX, BYTE regY)
	{
		if (V[regX] != V[regY])
		{
			pc += 0x04;

#ifndef SUPPRESS_PROC_INFO
			std::cout << "Skipped instruction because V" << (WORD)regX << " != V" << (WORD)regY << std::endl;
#endif
		}
		else
		{
			pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
			std::cout << "Didn't skip instruction because V" << (WORD)regX << " == V" << (WORD)regY << std::endl;
#endif
		}

	}


	///////////////////0xANNN///////////////////
	/// \brief Sets I to NNN
	///
	/// \param address NNN
	////////////////////////////////////////////
	void LD(WORD address)
	{
		I = address;
		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Set I to 0x" << address << std::endl;
#endif
	}


	///////////////////0xBNNN///////////////////
	/// \brief Jump to location NNN + V0
	///
	/// \param address NNN
	////////////////////////////////////////////
	void JP_V(WORD address)
	{
		pc = address + V[0x0];

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Jumped to adress " << (WORD)pc << std::endl;
#endif
	}

	
	///////////////////0xCXKK///////////////////
	/// \brief Sets VX to a random byte ANDed with KK
	///
	/// \param regX X
	/// \param byte KK
	////////////////////////////////////////////
	void RND(BYTE regX, BYTE byte)
	{
		static std::default_random_engine engine(std::chrono::system_clock::now().time_since_epoch().count());
		static std::uniform_int_distribution<WORD> range(0, 0xFF);

		BYTE rnd = range(engine) & byte;

		V[regX] = rnd;

		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "(Random) Set V" << (WORD)regX << " to 0x" << (WORD)rnd << std::endl;
#endif
	}


	///////////////////0xDXYN///////////////////
	/// \brief Draws sprite that is N bytes long and
	///        stored at I at position (VX, VY). Set
	///        VF = 1 if collision
	///
	/// \param regX		x
	/// \param regY		y
	/// \param bytes	N
	////////////////////////////////////////////
	void DRW(BYTE regX, BYTE regY, BYTE bytes)
	{
		V[0xF] = 0x00;

		for (BYTE y = 0; y < bytes; y++)
		{
			BYTE line = memory[I + y];

			for (BYTE x = 0; x < 8; x++)
			{
				BYTE pixel = line & (0x80 >> x);
				if (pixel != 0)
				{
					BYTE totalX = V[regX] + x;
					BYTE totalY = V[regY] + y;
					WORD index = totalY * 64 + totalX;

					if (gfx[index] == 1) {
						V[0xF] = 1;
					}

					gfx[index] ^= 1;
				}
				
			}
		}

		pc += 0x02;
		drawFlag = true;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Drew Sprite at position (V" << (WORD)regX << ", V" << (WORD)regY << ")[" << std::dec << (WORD)V[regX] << ", " << (WORD)V[regY] << "]" << std::endl;
#endif
	}


	///////////////////0xEX9E///////////////////
	/// \brief Skip instruction if key of value VX is pressed
	///
	/// \param regX X 
	///
	////////////////////////////////////////////
	void SKP(BYTE regX)
	{
		if (GetKeyState(keymap.find(V[regX])->second) & 0x8000)
		{
			pc += 0x04;

#ifndef SUPPRESS_PROC_INFO
			std::cout << "Skipped instruction because key " << (WORD)V[regX] << " was pressed" << std::endl;
#endif	
		}
		else
		{
			pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
			std::cout << "Didn't skip instruction because key " << (WORD)V[regX] << " was not pressed" << std::endl;
#endif
		}
	}


	///////////////////0xEXA1///////////////////
	/// \brief Skip instruction if key of value VX is not pressed
	///
	/// \param regX X 
	///
	////////////////////////////////////////////
	void SKNP(BYTE regX)
	{
		if (GetKeyState(keymap.find(V[regX])->second) & 0x8000)
		{
			pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
			std::cout << "Didn't skip instruction because key " << (WORD)V[regX] << " was pressed" << std::endl;
#endif
		} 
		else
		{
			pc += 0x04;

#ifndef SUPPRESS_PROC_INFO
			std::cout << "Skipped instruction because key " << (WORD)V[regX] << " was not pressed" << std::endl;
#endif
		}
	}


	///////////////////0xFX07///////////////////
	/// \brief Store BCD representation of X at I
	///
	/// \param regX X 
	///
	////////////////////////////////////////////
	void LD_X(BYTE regX)
	{
		V[regX] = delay_timer;

		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Set V" << (WORD)regX << " to delay_timer(" << (WORD)delay_timer << ")" << std::endl;
#endif
	}


	///////////////////0xFX0A///////////////////
	/// \brief Halt program until key press. Save key to VX
	///
	/// \param regX X 
	///
	////////////////////////////////////////////
	void LD_K(BYTE regX)
	{
#ifndef SUPPRESS_PROC_INFO
		std::cout << "Waiting for Key press..." << std::endl;
#endif
		for (BYTE key = 0x0; key < 0xF; key++)
		{
			if (GetKeyState(keymap.find(key)->second) & 0x8000)
			{
				V[regX] = key;
				pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
				std::cout << "Key pressed: 0x" << (WORD)key << ". Saved to V" << (WORD)regX << std::endl;
#endif

				break;
			}
		}
	}


	///////////////////0xFX15///////////////////
	/// \brief Set delay timer to VX
	///
	/// \param regX X 
	///
	////////////////////////////////////////////
	void LD_DT(BYTE regX)
	{
		delay_timer = V[regX];

		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Set delay timer to " << (WORD)V[regX] << std::endl;
#endif
	}


	///////////////////0xFX18///////////////////
	/// \brief Set sound timer to VX
	///
	/// \param regX X 
	///
	////////////////////////////////////////////
	void LD_ST(BYTE regX)
	{
		sound_timer = V[regX];

		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Set sound timer to " << (WORD)V[regX] << std::endl;
#endif
	}


	///////////////////0xFX1E///////////////////
	/// \brief Set I = I + VX
	///
	/// \param regX X 
	///
	////////////////////////////////////////////
	void ADD_I(BYTE regX)
	{
		I += V[regX];
		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Added V" << (WORD)regX << "(" << (WORD)V[regX] << ") to I. I = " << (WORD)I << std::endl;
#endif
	}

	///////////////////0xFX29///////////////////
	/// \brief Set I to location of font for digit VX
	///
	/// \param regX X 
	///
	////////////////////////////////////////////
	void LD_F(BYTE regX)
	{
		I = 0x0000 + (V[regX] * 5);
		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Stored font \"" << (WORD)V[regX] << "\" at I" << std::endl;
#endif
	}


	///////////////////0xFX33///////////////////
	/// \brief Store BCD representation of X at I
	///
	/// \param regX X 
	///
	////////////////////////////////////////////
	void LD_B(BYTE regX)
	{
		BYTE value = V[regX];

		BYTE hundreds = (value - (value % 100)) / 100;
		value -= hundreds * 100;

		BYTE tens = (value - (value % 10)) / 10;
		value -= tens * 10;

		memory[I] = hundreds;
		memory[I + 1] = tens;
		memory[I + 2] = value;

		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Storing Binary Coded Decimal V" << (WORD)regX << " as {" << std::dec << (WORD)hundreds << ", " << (WORD)tens << ", " << (WORD)value << "}" << std::endl;
#endif
	}


	///////////////////0xFX55///////////////////
	/// \brief Fill memory at I with values from V0 - VX
	///
	/// \param regX X 
	///
	////////////////////////////////////////////
	void LD_55(BYTE regX)
	{
		for (int offset = 0; offset <= regX; offset++)
		{
			memory[I + offset] = V[offset];
		}

		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Filled memory starting at 0x" << (WORD)I << " with the values of V0 to V" << (WORD)regX << std::endl;
#endif
	}


	///////////////////0xFX65///////////////////
	/// \brief Fill V0 - VX with memory data starting at I
	///
	/// \param regX X 
	///
	////////////////////////////////////////////
	void LD_65(BYTE regX)
	{
		for (int offset = 0; offset <= regX; offset++)
		{
			V[offset] = memory[I + offset];
		}

		pc += 0x02;

#ifndef SUPPRESS_PROC_INFO
		std::cout << "Filled V0 through V" << (WORD)regX << " with memory values starting at 0x" << I << std::endl;
#endif
	}
} chip8;




class olcConsoleGameEngine
{
public:
	olcConsoleGameEngine()
	{
		m_nScreenWidth = 80;
		m_nScreenHeight = 30;

		m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		m_hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);

		m_sAppName = L"Default";
	}

	int ConstructConsole(int width, int height, int fontw, int fonth)
	{
		if (m_hConsole == INVALID_HANDLE_VALUE)
			return Error(L"Bad Handle");

		m_nScreenWidth = width;
		m_nScreenHeight = height;

		m_rectWindow = { 0, 0, 1, 1 };
		SetConsoleWindowInfo(m_hConsole, TRUE, &m_rectWindow);

		// Set the size of the screen buffer
		COORD coord = { (short)m_nScreenWidth, (short)m_nScreenHeight };
		if (!SetConsoleScreenBufferSize(m_hConsole, coord))
			Error(L"SetConsoleScreenBufferSize");

		// Assign screen buffer to the console
		if (!SetConsoleActiveScreenBuffer(m_hConsole))
			return Error(L"SetConsoleActiveScreenBuffer");

		// Set the font size now that the screen buffer has been assigned to the console
		CONSOLE_FONT_INFOEX cfi;
		cfi.cbSize = sizeof(cfi);
		cfi.nFont = 0;
		cfi.dwFontSize.X = fontw;
		cfi.dwFontSize.Y = fonth;
		cfi.FontFamily = FF_DONTCARE;
		cfi.FontWeight = FW_NORMAL;

		/*	DWORD version = GetVersion();
		DWORD major = (DWORD)(LOBYTE(LOWORD(version)));
		DWORD minor = (DWORD)(HIBYTE(LOWORD(version)));*/

		//if ((major > 6) || ((major == 6) && (minor >= 2) && (minor < 4)))		
		//	wcscpy_s(cfi.FaceName, L"Raster"); // Windows 8 :(
		//else
		//	wcscpy_s(cfi.FaceName, L"Lucida Console"); // Everything else :P

		//wcscpy_s(cfi.FaceName, L"Liberation Mono");
		wcscpy_s(cfi.FaceName, L"Consolas");
		if (!SetCurrentConsoleFontEx(m_hConsole, false, &cfi))
			return Error(L"SetCurrentConsoleFontEx");

		// Get screen buffer info and check the maximum allowed window size. Return
		// error if exceeded, so user knows their dimensions/fontsize are too large
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		if (!GetConsoleScreenBufferInfo(m_hConsole, &csbi))
			return Error(L"GetConsoleScreenBufferInfo");
		if (m_nScreenHeight > csbi.dwMaximumWindowSize.Y)
			return Error(L"Screen Height / Font Height Too Big");
		if (m_nScreenWidth > csbi.dwMaximumWindowSize.X)
			return Error(L"Screen Width / Font Width Too Big");

		// Set Physical Console Window Size
		m_rectWindow = { 0, 0, (short)m_nScreenWidth - 1, (short)m_nScreenHeight - 1 };
		if (!SetConsoleWindowInfo(m_hConsole, TRUE, &m_rectWindow))
			return Error(L"SetConsoleWindowInfo");

		// Set flags to allow mouse input		
		if (!SetConsoleMode(m_hConsoleIn, ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT))
			return Error(L"SetConsoleMode");

		// Allocate memory for screen buffer
		m_bufScreen = new CHAR_INFO[m_nScreenWidth*m_nScreenHeight];
		memset(m_bufScreen, 0, sizeof(CHAR_INFO) * m_nScreenWidth * m_nScreenHeight);

		SetConsoleCtrlHandler((PHANDLER_ROUTINE)CloseHandler, TRUE);
		return 1;
	}

	virtual void Draw(int x, int y, short c = 0x2588, short col = 0x000F)
	{
		if (x >= 0 && x < m_nScreenWidth && y >= 0 && y < m_nScreenHeight)
		{
			m_bufScreen[y * m_nScreenWidth + x].Char.UnicodeChar = c;
			m_bufScreen[y * m_nScreenWidth + x].Attributes = col;
		}
	}

	void Clip(int &x, int &y)
	{
		if (x < 0) x = 0;
		if (x >= m_nScreenWidth) x = m_nScreenWidth;
		if (y < 0) y = 0;
		if (y >= m_nScreenHeight) y = m_nScreenHeight;
	}

	~olcConsoleGameEngine()
	{
		SetConsoleActiveScreenBuffer(m_hOriginalConsole);
		delete[] m_bufScreen;
	}

public:
	void Start()
	{
		// Start the thread
		m_bAtomActive = true;
		std::thread t = std::thread(&olcConsoleGameEngine::GameThread, this);

		// Wait for thread to be exited
		t.join();
	}

	int ScreenWidth()
	{
		return m_nScreenWidth;
	}

	int ScreenHeight()
	{
		return m_nScreenHeight;
	}

private:
	void GameThread()
	{
		// Create user resources as part of this thread
		if (!OnUserCreate())
			m_bAtomActive = false;

		auto tp1 = std::chrono::system_clock::now();
		auto tp2 = std::chrono::system_clock::now();

		while (m_bAtomActive)
		{
			// Run as fast as possible
			while (m_bAtomActive)
			{
				// Handle Timing
				tp2 = std::chrono::system_clock::now();
				std::chrono::duration<float> elapsedTime = tp2 - tp1;
				tp1 = tp2;
				float fElapsedTime = elapsedTime.count();


				// Handle Frame Update
				if (!OnUserUpdate(fElapsedTime))
					m_bAtomActive = false;

				// Update Title & Present Screen Buffer
				wchar_t s[256];
				swprintf_s(s, 256, L"OneLoneCoder.com - Console Game Engine - %s - FPS: %3.2f", m_sAppName.c_str(), 1.0f / fElapsedTime);
				SetConsoleTitle(s);
				WriteConsoleOutput(m_hConsole, m_bufScreen, { (short)m_nScreenWidth, (short)m_nScreenHeight }, { 0,0 }, &m_rectWindow);
			}

			if (m_bEnableSound)
			{
				// Close and Clean up audio system
			}

			// Allow the user to free resources if they have overrided the destroy function
			if (OnUserDestroy())
			{
				// User has permitted destroy, so exit and clean up
				delete[] m_bufScreen;
				SetConsoleActiveScreenBuffer(m_hOriginalConsole);
				m_cvGameFinished.notify_one();
			}
			else
			{
				// User denied destroy for some reason, so continue running
				m_bAtomActive = true;
			}
		}
	}

public:
	// User MUST OVERRIDE THESE!!
	virtual bool OnUserCreate() = 0;
	virtual bool OnUserUpdate(float fElapsedTime) = 0;

	// Optional for clean up 
	virtual bool OnUserDestroy() { return true; }

	protected:
		int Error(const wchar_t *msg)
		{
			wchar_t buf[256];
			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 256, NULL);
			SetConsoleActiveScreenBuffer(m_hOriginalConsole);
			wprintf(L"ERROR: %s\n\t%s\n", msg, buf);
			return 0;
		}

		static BOOL CloseHandler(DWORD evt)
		{
			// Note this gets called in a seperate OS thread, so it must
			// only exit when the game has finished cleaning up, or else
			// the process will be killed before OnUserDestroy() has finished
			if (evt == CTRL_CLOSE_EVENT)
			{
				m_bAtomActive = false;

				// Wait for thread to be exited
				std::unique_lock<std::mutex> ul(m_muxGame);
				m_cvGameFinished.wait(ul);
			}
			return true;
		}

protected:
	int m_nScreenWidth;
	int m_nScreenHeight;
	CHAR_INFO *m_bufScreen;
	std::wstring m_sAppName;
	HANDLE m_hOriginalConsole;
	CONSOLE_SCREEN_BUFFER_INFO m_OriginalConsoleInfo;
	HANDLE m_hConsole;
	HANDLE m_hConsoleIn;
	SMALL_RECT m_rectWindow;
	short m_keyOldState[256] = { 0 };
	short m_keyNewState[256] = { 0 };
	bool m_mouseOldState[5] = { 0 };
	bool m_mouseNewState[5] = { 0 };
	bool m_bConsoleInFocus = true;
	bool m_bEnableSound = false;

	// These need to be static because of the OnDestroy call the OS may make. The OS
	// spawns a special thread just for that
	static std::atomic<bool> m_bAtomActive;
	static std::condition_variable m_cvGameFinished;
	static std::mutex m_muxGame;
};

// Define our static variables
std::atomic<bool> olcConsoleGameEngine::m_bAtomActive(false);
std::condition_variable olcConsoleGameEngine::m_cvGameFinished;
std::mutex olcConsoleGameEngine::m_muxGame;


//////////////////////////////////////////////
/// \brief Initializes and handles Screen
///
//////////////////////////////////////////////
class Screen : public olcConsoleGameEngine
{
public:
	Screen(){}

	virtual bool OnUserCreate()
	{
		then = std::chrono::system_clock::now();

		chip8.Initialize();
		chip8.LoadGame(FILENAME);
		//MessageBox(NULL, L"", L"", MB_OK);

		return true;
	}

	virtual bool OnUserUpdate(float elapsedTime)
	{
		chip8.EmulateCycle();

		now = std::chrono::system_clock::now();

		if (std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() - std::chrono::duration_cast<std::chrono::milliseconds>(then.time_since_epoch()).count() >= 1000 / 60)
		{
			//std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() - std::chrono::duration_cast<std::chrono::milliseconds>(then.time_since_epoch()).count() << std::endl;

			if (chip8.delay_timer != 0)
			{
				chip8.delay_timer--;
			}
			if (chip8.sound_timer != 0) {
				chip8.sound_timer--;
			}

			then = now;

		}

		if (chip8.drawFlag)
		{
			drawGraphics();
		}
		Sleep(1);

		return true;
	}

private:
	std::chrono::system_clock::time_point then, now;

	//////////////////////////////////////////////
	/// \brief Draws the pixel array to the screen
	///
	/// \param window The render target
	/// \param pixels The pixel array
	///
	//////////////////////////////////////////////
	void drawGraphics()
	{
		BYTE* gfx = chip8.getDisplay();

		for (int i = 0; i < WIDTH * HEIGHT; i++)
		{
			int y = floor(i / WIDTH);
			int x = i - (y * WIDTH);
			COLOUR col = (gfx[i] == 0x00) ? FG_BLACK : FG_WHITE;

			std::wstring s = std::to_wstring(x) + L", " + std::to_wstring(y);

			Draw(x, y, PIXEL_SOLID, col);
		}

		chip8.drawFlag = false;
	}
};



int main(int argc, char** argv)
{
	Screen screen;
	screen.ConstructConsole(WIDTH, HEIGHT, 16, 16);
	screen.Start();

	

	return 0;
}
