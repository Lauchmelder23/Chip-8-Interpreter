#include <array>
#include <fstream>
#include <iostream>
#include <random>
#include <chrono>
#include <unordered_map>

#include <SFML/Graphics.hpp>

typedef unsigned short WORD;
typedef unsigned char BYTE;

const constexpr unsigned WIDTH = 64;
const constexpr unsigned HEIGHT = 32;
const constexpr unsigned RAM = 4096; // 4kB RAM
const constexpr unsigned FONTSET_SIZE = 16 * 5;
const constexpr unsigned SCALE = 20;

sf::RenderWindow window(sf::VideoMode(WIDTH * SCALE, HEIGHT * SCALE), "CHIP-8 Interpreter, x86", sf::Style::Close);
sf::Event event;
sf::Uint8* pixels = new sf::Uint8[WIDTH * HEIGHT * 4]{};



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

std::unordered_map<BYTE, sf::Keyboard::Key> keymap =
{
	{ 0x1, sf::Keyboard::Num1	},{ 0x2, sf::Keyboard::Num2 },{ 0x3, sf::Keyboard::Num3 },{ 0xC, sf::Keyboard::Num4 },
	{ 0x4, sf::Keyboard::Q		},{ 0x5, sf::Keyboard::W	},{ 0x6, sf::Keyboard::E	},{ 0xD, sf::Keyboard::R	},
	{ 0x7, sf::Keyboard::A		},{ 0x8, sf::Keyboard::S	},{ 0x9, sf::Keyboard::D	},{ 0xE, sf::Keyboard::F	},
	{ 0xA, sf::Keyboard::Y		},{ 0x0, sf::Keyboard::X	},{ 0xB, sf::Keyboard::C	},{ 0xF, sf::Keyboard::V	}
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
		std::cout << std::uppercase << std::hex << opcode << ": ";

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

		std::cout << "Screen cleared" << std::endl;
	}


	///////////////////0x00EE///////////////////
	/// \brief Returns from subroutine
	///
	////////////////////////////////////////////
	void RET()
	{
		pc = stack[--sp] + 0x02;
		std::cout << "Returned from subroutine" << std::endl;
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
		std::cout << "Jumped to 0x" << address << std::endl;
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

		std::cout << "Called subroutine at 0x" << address << std::endl;
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
			std::cout << "Skipped instruction because V" << (WORD)regX << " == " << (WORD)kk << std::endl;
		}
		else 
		{
			pc += 0x02;
			std::cout << "Didn't skip instruction because V" << (WORD)regX << " != " << (WORD)kk << std::endl;
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
			std::cout << "Skipped instruction because V" << (WORD)regX << " != " << (WORD)kk << std::endl;
		}
		else
		{
			pc += 0x02;
			std::cout << "Didn't skip instruction because V" << (WORD)regX << " == " << (WORD)kk << std::endl;
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

		std::cout << "Set V" << (WORD)regX << " to 0x" << (WORD)byte << std::endl;
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

		std::cout << "Added 0x" << (WORD)byte << " to V" << (WORD)regX << std::endl;
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

		std::cout << "V" << (WORD)regY << "(0x" << (WORD) V[regY] << ") => V" << (WORD)regX << std::endl;
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

		std::cout << "V" << (WORD)regX << " |= V" << (WORD)regY << "(0x" << (WORD)V[regY] << ") => " << (WORD)V[regX] << std::endl;
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

		std::cout << "V" << (WORD)regX << " &= V" << (WORD)regY << "(0x" << (WORD)V[regY] << ") => " << (WORD)V[regX] << std::endl;
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

		std::cout << "V" << (WORD)regX << " ^= V" << (WORD)regY << "(0x" << (WORD)V[regY] << ") => " << (WORD)V[regX] << std::endl;
	}


	//////////////////0x8XY4///////////////////
	/// \brief VX = VX + VY, VF = carry
	///
	/// \param regX X
	/// \param regY Y
	////////////////////////////////////////////
	void ADD_XY(BYTE regX, BYTE regY)
	{
		if (V[regY] > 0xF - V[regX])
			V[0xF] = 1;
		else
			V[0xF] = 0;

		V[regX] = (V[regX] + V[regY]) & 0xFF;
		pc += 0x02;

		std::cout << "V" << (WORD)regX << " += V" << (WORD)regY << "(0x" << (WORD)V[regY] << ") => " << (WORD)V[regX] << ", Carry = " << (WORD)V[0xF] << std::endl;
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

		std::cout << "V" << (WORD)regX << " -= V" << (WORD)regY << "(0x" << (WORD)V[regY] << ") => " << (WORD)V[regX] << ", Carry = " << (WORD)V[0xF] << std::endl;
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
		std::cout << "Shifted V" << (WORD)regX << " right once. VF = " << (WORD)V[0xF] << std::endl;
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

		std::cout << "V" << (WORD)regX << " = V" << (WORD)regY << "(0x" << (WORD)V[regY] << ") - V" << (WORD)regX << " => " << (WORD)V[regX] << ", Carry = " << (WORD)V[0xF] << std::endl;
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
		std::cout << "Shifted V" << (WORD)regX << " left once. VF = " << (WORD)V[0xF] << std::endl;
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
			std::cout << "Skipped instruction because V" << (WORD)regX << " != V" << (WORD)regY << std::endl;
		}
		else
		{
			pc += 0x02;
			std::cout << "Didn't skip instruction because V" << (WORD)regX << " == V" << (WORD)regY << std::endl;
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

		std::cout << "Set I to 0x" << address << std::endl;
	}


	///////////////////0xBNNN///////////////////
	/// \brief Jump to location NNN + V0
	///
	/// \param address NNN
	////////////////////////////////////////////
	void JP_V(WORD address)
	{
		pc = address + V[0x0];

		std::cout << "Jumped to adress " << (WORD)pc << std::endl;
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

		std::cout << "(Random) Set V" << (WORD)regX << " to 0x" << (WORD)rnd << std::endl;
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

		std::cout << "Drew Sprite at position (V" << (WORD)regX << ", V" << (WORD)regY << ")[" << std::dec << (WORD)V[regX] << ", " << (WORD)V[regY] << "]" << std::endl;
	}


	///////////////////0xEX9E///////////////////
	/// \brief Skip instruction if key of value VX is pressed
	///
	/// \param regX X 
	///
	////////////////////////////////////////////
	void SKP(BYTE regX)
	{
		if (!sf::Keyboard::isKeyPressed(keymap.find(V[regX])->second))
		{
			pc += 0x02;
			std::cout << "Didn't skip instruction because key " << (WORD)V[regX] << " was not pressed" << std::endl;
		}
		else
		{
			pc += 0x04;
			std::cout << "Skipped instruction because key " << (WORD)V[regX] << " was pressed" << std::endl;
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
		if (sf::Keyboard::isKeyPressed(keymap.find(V[regX])->second))
		{
			pc += 0x02;
			std::cout << "Didn't skip instruction because key " << (WORD)V[regX] << " was pressed" << std::endl;
		} 
		else
		{
			pc += 0x04;
			std::cout << "Skipped instruction because key " << (WORD)V[regX] << " was not pressed" << std::endl;
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
		std::cout << "Set V" << (WORD)regX << " to delay_timer(" << (WORD)delay_timer << ")" << std::endl;
	}


	///////////////////0xFX0A///////////////////
	/// \brief Halt program until key press. Save key to VX
	///
	/// \param regX X 
	///
	////////////////////////////////////////////
	void LD_K(BYTE regX)
	{
		std::cout << "Waiting for Key press..." << std::endl;
		for (BYTE key = 0x0; key < 0xF; key++)
		{
			if (sf::Keyboard::isKeyPressed(keymap.find(key)->second))
			{
				V[regX] = key;
				pc += 0x02;

				std::cout << "Key pressed: 0x" << (WORD)key << ". Saved to V" << (WORD)regX << std::endl;
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
		std::cout << "Set delay timer to " << (WORD)V[regX] << std::endl;
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
		std::cout << "Set sound timer to " << (WORD)V[regX] << std::endl;
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

		std::cout << "Added V" << (WORD)regX << "(" << (WORD)V[regX] << ") to I. I = " << (WORD)I << std::endl;
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

		std::cout << "Stored font \"" << (WORD)V[regX] << "\" at I" << std::endl;
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
		std::cout << "Storing Binary Coded Decimal V" << (WORD)regX << " as {" << std::dec << (WORD)hundreds << ", " << (WORD)tens << ", " << (WORD)value << "}" << std::endl;
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
		std::cout << "Filled memory starting at 0x" << (WORD)I << " with the values of V0 to V" << (WORD)regX << std::endl;
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
		std::cout << "Filled V0 through V" << (WORD)regX << " with memory values starting at 0x" << I << std::endl;
	}
} chip8;

//////////////////////////////////////////////
/// \brief Draws the pixel array to the screen
///
/// \param window The render target
/// \param pixels The pixel array
///
//////////////////////////////////////////////
void drawGraphics()
{
	static sf::Texture texture;
	static sf::Sprite sprite;

	BYTE* gfx = chip8.getDisplay();

	for (int i = 0; i < WIDTH * HEIGHT; i++)
	{
		//printf("%i: %i\n", i, gfx[i]);
		pixels[4 * i + 0] = 255 * gfx[i];
		pixels[4 * i + 1] = 255 * gfx[i];
		pixels[4 * i + 2] = 255 * gfx[i];
		pixels[4 * i + 3] = 255 * gfx[i];
	}

	texture.create(WIDTH, HEIGHT);
	texture.update(pixels);

	sprite.setTexture(texture);
	sprite.setScale(sf::Vector2f(SCALE, SCALE));

	window.clear(sf::Color::Black);
	window.draw(sprite);
	window.display();

	chip8.drawFlag = false;
}

int main(int argc, char** argv)
{
	sf::Clock timer;
	timer.restart();

	chip8.Initialize();
	chip8.LoadGame("pong2.c8");

	while (!chip8.interrupt)
	{
		chip8.EmulateCycle();

		if (timer.getElapsedTime().asMilliseconds() >= 1000 / 60)
		{
			if (chip8.delay_timer != 0) chip8.delay_timer--;
			if (chip8.sound_timer != 0) chip8.sound_timer--;
			timer.restart();
		}

		if (chip8.drawFlag)
		{
			drawGraphics();
		}

		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
			{
				chip8.interrupt = true;
				window.close();
			}
			
		}
		sf::sleep(sf::milliseconds(1));
	}

	return 0;
}
