#include "pch.h"

#include "Chip8.h"

#include "Log.h"

using namespace logging;

Chip8::Chip8()
{
	Initialize();
}

Chip8::Chip8(const char* filename)
{
	Initialize();
	LoadProgram(filename);
}

void Chip8::Initialize()
{
	memset(_memory, 0, TotalMemoryBytes);
	
	memset(_v, 0, NumRegisers);
	_i = 0;
	_pc = ProgramStart;

	memset(_gfx, 0, ScreenWidth * ScreenHeight);

	_delayTimer = 0;
	_soundTimer = 0;

	//_opcode = 0;
	memset(_stack, 0, StackSize);
	_sp = 0;

	memset(_keys, 0, NumKeys);

	// Load fontset
	memcpy(_memory, Font, NumFontChars);

	_draw = false;
}

unsigned char Chip8::Register(unsigned int reg) const
{
	if (reg >= NumRegisers)
	{
		return 0;
	}

	return _v[reg];
}

unsigned char Chip8::Memory(unsigned int loc) const
{
	if (loc >= TotalMemoryBytes)
	{
		return 0;
	}

	return _memory[loc];
}

unsigned char Chip8::Pixel(int x, int y) const
{
	if (x < 0 || x >= ScreenWidth || y < 0 || y > ScreenHeight)
	{
		Info("Error: pixel out of range (%d, %d)", x, y);
		return 0;
	}

	return _gfx[(ScreenHeight * y) + x];
}

bool Chip8::LoadProgram(const char* filename) 
{
	size_t size = ReadProgram(filename, &(_memory[ProgramStart]), TotalMemoryBytes - ProgramStart);
	if (size == 0)
	{
		Info("Error: could not load program %s\n", filename);
		return false;
	}
	
	return true;
}

size_t Chip8::ReadProgram(const char* filename, unsigned char* buffer, size_t bufferSize)
{
	assert(filename != nullptr);
	assert(buffer != nullptr);
	
	FILE* file = nullptr;
	if (fopen_s(&file, filename, "rb") != 0)
	{
		Info("Error: Could not open file %s\n", filename);
		return 0;
	}
	
	size_t read = fread(buffer, 1, bufferSize, file);
	if (!feof(file))
	{
		Info("Error: could not read the whole program into buffer of size %zu\n", bufferSize);
		return 0;
	}

	if (read == 0)
	{
		Info("Error: unable to read program %s\n", filename); 
		return 0;
	}
	else
	{
		Info("Read program %s (%zu)", filename, read);
	}

	return read;
}

void Chip8::EmulateCycle()
{
	_draw = false;

	// fetch and execute the next op code
	unsigned short opCode = (_memory[_pc] << 8 | _memory[_pc + 1]);

	// execute the instruction
	Info("processing opCode %04x...", opCode);
	switch (opCode & 0xF000)
	{
		case 0x0000:
		{
			switch(opCode & 0x00FF)
			{
				// 00E0 	Display 	disp_clear() 	Clears the screen
				/*case 0x00E0:
				{
					break;
				}*/
 
				// 00EE 	Flow 	return; 	Returns from a subroutine 
				case 0x000E: 
				{
					_pc = _stack[_sp--];
					_pc += 2;
					break;
				}
				break;
 
				default:
				{
					Info("Error: Unknown opCode %04x!", opCode);
					break;
				}
			}
			break;
		}

		// 2NNN 	Flow 	*(0xNNN)() 	Calls subroutine at NNN
		case 0x2000:
		{
			_stack[_sp++] = _pc;
			_pc = opCode & 0x0FFF;
			break;
		}

		// 6XNN 	Const 	Vx = NN 	Sets VX to NN
		case 0x6000:
		{
			unsigned short v = (opCode & 0x0F00) >> 8;
			unsigned char val = (opCode & 0x00FF);
			assert(v < NumRegisers);
			_v[v] = val;
			
			_pc += 2;
			break;
		}

		case 0x8000:
		{
			unsigned short x = (opCode & 0x0F00) >> 8;
			unsigned short y = (opCode & 0x00F0) >> 4;
			switch (opCode & 0x000F)
			{
				/*.

..
8XY5 	Math 	Vx -= Vy 	VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there isn't.
8XY6[a] 	BitOp 	Vx>>=1 	Stores the least significant bit of VX in VF and then shifts VX to the right by 1.[b]
8XY7[a] 	Math 	Vx=Vy-Vx 	Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there isn't.
8XYE[a] 	BitOp 	Vx<<=1 	Stores the most significant bit of VX in VF and then shifts VX to the left by 1.[b]*/

				// 8XY0 	Assign 	Vx=Vy 	Sets VX to the value of VY
				case 0x0000:
				{
					_v[x] = _v[y];
					break;
				}

				// 8XY1 	BitOp 	Vx=Vx|Vy 	Sets VX to VX or VY. (Bitwise OR operation)
				case 0x0001:
				{
					_v[x] = _v[x] | _v[y];
					break;
				}

				// 8XY2 	BitOp 	Vx=Vx&Vy 	Sets VX to VX and VY. (Bitwise AND operation)
				case 0x0002:
				{
					_v[x] = _v[x] & _v[y];
					break;
				}

				// 8XY3[a] 	BitOp 	Vx=Vx^Vy 	Sets VX to VX xor VY
				case 0x0003:
				{
					_v[x] = _v[x] ^ _v[y];
					break;
				}

				// 8XY4 	Math 	Vx += Vy 	Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there isn't
				case 0x0004:
				{
					_v[0xF] = (_v[y] > (0xFF - _v[x])) ? 1 : 0;
					_v[x] += _v[y];
					break;
				}

				/*case 0x0005:
				{
					
					break;
				}

				case 0x0006:
				{
					break;
				}

				case 0x0007:
				{
					break;
				}

				case 0x000E:
				{
					break;
				}*/

				default:
				{
					Info("Error: Unknown opCode %04x!", opCode);
					break;
				}
			}

			_pc += 2;
			break;
		}

		// ANNN 	MEM 	I = NNN 	Sets I to the address NNN
		case 0xA000:
		{
			_i = opCode & 0x0FFF;
			_pc += 2;
			break;
		}

		// DXYN 	Disp 	draw(Vx,Vy,N) 	Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels and a height of N pixels. Each row of 8 pixels is read as bit-coded starting from memory location I; I value doesn�t change after the execution of this instruction. As described above, VF is set to 1 if any screen pixels are flipped from set to unset when the sprite is drawn, and to 0 if that doesn�t happen 
		case 0xD000:
		{
			unsigned short x = (opCode & 0x0F00) >> 8;
			unsigned short y = (opCode & 0x00F0) >> 4;
			unsigned short rows = (opCode & 0x000F);

			_v[0xF] = 0;	// clear the carry bit

			for (int row = 0 ; row < rows ; ++row)
			{
				unsigned short pixel = _memory[_i + row];
				for (int col = 0 ; col < 8 ; ++col)
				{
					if ((pixel & (0x80 >> col)) != 0)	// if the pixel bit is set
					{
						int index = x + col + (y + row) * ScreenHeight;
						// if the pixel is already set then set the carry flag
						if (_gfx[index] != 0)
						{
							_v[0xF] = 1;
						}

						// set the pixel
						_gfx[index] ^= 1;
					}
				}
			}

			// TODO = set the draw flag

			_pc += 2;
			_draw = true;
			break;
		}

		case 0xE000:
		{
			unsigned short x = (opCode & 0x0F00) >> 8;

			switch (opCode & 0x00FF)
			{
				// EX9E 	KeyOp 	if(key()==Vx) 	Skips the next instruction if the key stored in VX is pressed. (Usually the next instruction is a jump to skip a code block)
				case 0x009E:
				{
					if (_keys[x] != 0)
					{
						_pc += 2;
					}

					break;
				}
				
				// EXA1 	KeyOp 	if(key()!=Vx) 	Skips the next instruction if the key stored in VX isn't pressed. (Usually the next instruction is a jump to skip a code block) 
				case 0x00A1:
				{
					if (_keys[x] == 0)
					{
						_pc += 2;
					}

					break;
				}

				default:
				{
					Info("Error: Unknown opCode %04x!", opCode);
					break;
				}
			}

			_pc += 2;
			break;
		}

		case 0xF000:
		{
//			FX07 	Timer 	Vx = get_delay() 	Sets VX to the value of the delay timer.
//FX0A 	KeyOp 	Vx = get_key() 	A key press is awaited, and then stored in VX. (Blocking Operation. All instruction halted until next key event)
//FX15 	Timer 	delay_timer(Vx) 	Sets the delay timer to VX.
//FX18 	Sound 	sound_timer(Vx) 	Sets the sound timer to VX.
//FX1E 	MEM 	I +=Vx 	Adds VX to I. VF is set to 1 when there is a range overflow (I+VX>0xFFF), and to 0 when there isn't.[c]
//FX29 	MEM 	I=sprite_addr[Vx] 	Sets I to the location of the sprite for the character in VX. Characters 0-F (in hexadecimal) are represented by a 4x5 font. 
//FX55 	MEM 	reg_dump(Vx,&I) 	Stores V0 to VX (including VX) in memory starting at address I. The offset from I is increased by 1 for each value written, but I itself is left unmodified.[d]
//FX65 	MEM 	reg_load(Vx,&I) 	Fills V0 to VX (including VX) with values from memory starting at address I. The offset from I is increased by 1 for each value written, but I itself is left unmodified.[d]

			unsigned short x = (opCode & 0x0F00) >> 8;
			switch (opCode & 0x00FF)
			{
				// // FX33 	BCD 	set_BCD(Vx); Stores the binary-coded decimal representation of VX, with the most significant of three digits at the address in I, the middle digit at I plus 1, and the least significant digit at I plus 2. (In other words, take the decimal representation of VX, place the hundreds digit in memory at location in I, the tens digit at location I+1, and the ones digit at location I+2.) 
				case 0x0033:
				{
					_memory[_i] = _v[x] / 100;
					_memory[_i + 1] = (_v[x] / 10) % 10;
					_memory[_i + 2] = (_v[x] % 100) % 10;

					break;
				}

			}
			
			_pc += 2;
			break;
		}

		default:
		{
			Info("Error: Unknown opCode %04x!", opCode);
			break;
		}
	}

	// TODO - update timers.
}