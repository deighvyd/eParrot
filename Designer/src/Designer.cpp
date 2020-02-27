#include "pch.h"

#include "Designer.h"

#include "Chip8.h"
#include "Log.h"

#include "imgui/imgui.h"

using namespace logging;

/*static */int Designer::DisplayWidth()
{
	return (Chip8::ScreenWidth * DisplayScale);
}

/*static */int Designer::DisplayHeight()
{
	return (Chip8::ScreenWidth * DisplayScale);
}
Designer::Designer()
	: Application(1280, 720)
{
	_programFile = "../assets/programs/pong";

	_chip8 = new Chip8(_programFile);
	
	_program = new unsigned char[Chip8::TotalMemoryBytes];
	_programSize = Chip8::ReadProgram(_programFile, _program, Chip8::TotalMemoryBytes);
	if (_programSize == 0)
	{
		MessageBox(_hWnd, L"Could not read program", L"Error", MB_OK);
	}	
}

Designer::~Designer()
{
	delete _chip8;
	_chip8 = nullptr;

	delete _program;
	_program = nullptr;

	delete _gfxTexture;
	_gfxTexture = nullptr;

	// TODO - release the texture memory?????
}

bool Designer::Initialize(LPCWSTR name)
{
	if (!Application::Initialize(name))
	{
		return false;
	}

	assert(_gfxTexture == nullptr);

	// allocate a buffer for the texture data
	size_t bufferSize = (DisplayWidth() * DisplayHeight()) * 4;
	_gfxTexture = new unsigned char[bufferSize];
	if (_gfxTexture == nullptr)
	{
		Info("Error: could not allocate memory for the gfx texture");
		return false;
	}
	memset(_gfxTexture, 0x00000000, bufferSize);
	
	return true;
}

bool Designer::DrawGfxTexture()
{
	// clear the texture
	size_t bufferSize = (DisplayWidth() * DisplayHeight()) * 4;
	memset(_gfxTexture, 0x00000000, bufferSize);

	// TODO - draw
	for (int y = 0 ; y < Chip8::ScreenHeight ; ++y)
	{
		for (int x = 0 ; x < Chip8::ScreenWidth ; ++x)
		{
			unsigned char pixel = _chip8->Pixel(x, y);

			int startX = (DisplayScale * x);
			int startY = (DisplayScale * y);
			for (int y2 = startY ; y2 < (startY + (int)DisplayScale) ; ++y2)
			{
				for (int x2 = startX ; x2 < (startX + (int)DisplayScale) ; ++x2)
				{
					int pixelIdx = (y2 * DisplayWidth()) + x2;
					if (pixel == 0)
					{
						_gfxTexture[pixelIdx] = 0x00000000;
					}
					else
					{
						_gfxTexture[pixelIdx] = 0xFFFFFFFF;
					}
				}
			}
		}
	}

	// TODO - release the old texture

	// build the texture
	GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
	_gfxTextureId = (ImTextureID)texture;

    // setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
    // upload pixels into texture
	// TODO - allow this to be regenerated
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, DisplayWidth(), DisplayHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, _gfxTexture);
	
	return true;
}

bool Designer::RunFrame()
{
	if (!Application::RunFrame())
	{
		return false;
	}

	_chip8->EmulateCycle(_paused && _step == 0);
	if (_step > 0)
	{
		--_step;
	}

	if (_chip8->Draw())
	{
		if (!DrawGfxTexture())
		{
			MessageBox(_hWnd, L"Failed to update the gfx texture", L"Error", MB_OK);
			// not fatal so do no bail
		}
	}

	return true;
}

void Designer::OnGui()
{
	if (ImGui::Begin("Program"))
	{
		if (ImGui::Button(_paused ? "Play" : "Pause"))
		{
			_paused = !_paused;
			_step = 0;
		}
		ImGui::SameLine();
		if (ImGui::Button("Step"))
		{
			if (_paused)
			{
				++_step;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset"))
		{
			_chip8->Initialize();
		}

		assert((_programSize % 2) == 0);
		ImGui::BeginChild("instructions");

		for (unsigned short pc = 0 ; pc < _programSize ; pc += 2)
		{
			bool active = pc == (_chip8->PC() - Chip8::ProgramStart);
			if (active)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, { 0.0f, 1.0f, 0.0f, 1.0f });
			}

			unsigned short opCode = (_program[pc] << 8 | _program[pc + 1]);
			ImGui::Text("%04d:\t0x%04X", (pc / 2) + 1, opCode);

			if (active)
			{
				ImGui::SetScrollHere();
				ImGui::PopStyleColor(1);
			}
		}

		ImGui::EndChild();
		ImGui::End();
	}

	if (ImGui::Begin("Registers"))
	{
		// TODO - binary view?
		ImGui::Text("I:\t0x%02X", _chip8->I());

		ImGui::Columns(4, 0, false);
		for (unsigned int reg = 0 ; reg < Chip8::NumRegisers ; ++reg)
		{
			ImGui::Text("V%X:\t0x%02X", reg, _chip8->Register(reg));
			ImGui::NextColumn();
		}
		ImGui::Columns(1);
		
		ImGui::End();
	}

	if (ImGui::Begin("Memory"))
	{
		// TODO - binary view?

		static constexpr int BytesPerRow = 16;
		
		unsigned int numColumns = (BytesPerRow / 4);
		unsigned int numRows = Chip8::TotalMemoryBytes / BytesPerRow;

		ImGui::Columns(numColumns + 1, 0, false);
		for (unsigned int row = 0 ; row < numRows ; ++row)
		{
			ImGui::Text("%04X:", (row * BytesPerRow));
			ImGui::NextColumn();

			for (unsigned int col = 0 ; col < numColumns; ++col)
			{
				unsigned int loc = (row * BytesPerRow) + col;
				ImGui::Text("0x%02X%02X%02X%02X", 
							_chip8->Memory(loc), 
							_chip8->Memory(loc + 1), 
							_chip8->Memory(loc + 2), 
							_chip8->Memory(loc + 3));
				ImGui::NextColumn();
			}
		}
		ImGui::Columns(1);
		
		ImGui::End();
	}

	if (ImGui::Begin("Gfx"))
	{
		for (unsigned int y = 0 ; y < Chip8::ScreenHeight ; ++y)
		{
			char line[Chip8::ScreenWidth + 1];
			for (unsigned int x = 0 ; x < Chip8::ScreenWidth ; ++x)
			{
				line[x] = _chip8->Pixel(x, y) == 0 ? '0' : '1';
			}
			line[Chip8::ScreenWidth] = '\0';
			ImGui::Text("%s", line);
		}

		ImGui::End();
	}

	if (ImGui::Begin("Emulator"))
	{
		ImGui::Image(_gfxTextureId, { Chip8::ScreenWidth * DisplayScale, Chip8::ScreenHeight * DisplayScale});
		ImGui::End();
	}
}
