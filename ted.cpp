// cbor_test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#define _CRT_SECURE_NO_WARNINGS
#define _CRTDBG_MAP_ALLOC  
#include "stdint.h"
#include "stdafx.h"
#include "windows.h"
#include "LOG.H"		// Capture putchar & printf output to a log file
#include "TMR.h"
//#include <iostream>
//using namespace std;

#ifdef _DEBUG
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
// Replace _NORMAL_BLOCK with _CLIENT_BLOCK if you want the
// allocations to be of _CLIENT_BLOCK type
#else
#define DBG_NEW new
#endif

#undef TRACE
#define TRACE(fmt,...) do { if (dflag) printf(fmt,__VA_ARGS__); } while(0)
//#define TRACE(...)
#undef DEBUG
#define DEBUG(...)

extern void MEM_Dump(unsigned char* data, int len, long base);


#undef MEM_Trace
#define MEM_Trace(data,len,base) do { if (dflag) MEM_Dump(data,len,base); } while(0)
//#define MEM_Trace(fmt,...)


char qflag = 0;  						// quiet mode off (output to window)

extern "C"
{
	void MEM_Dump(uint8_t* data, uint16_t len, uint32_t base)
	{
		uint16_t i, j;

		//if (!CFG_IsTrace(DFLAG_TRC))
		//	return;

		//CON_printf("MEM: @%08x len=%04x\n",data,len);
		for (i = 0; i < len; i += 16)
		{
			printf(" %06x: ", base + i);
			for (j = 0; j < 16; j++)
			{
				if (j != 0)
				{
					if (!(j % 8))
						printf(" ");
					if (!(j % 1))
						printf(" ");
				}
				if ((i + j) < len)
					printf("%02x", data[i + j]);
				else
					printf("  ");
			}
			printf("  ");
			for (j = 0; j < 16 && (i + j) < len; j++)
			{
				if ((i + j) < len)
				{
					if (isprint(data[i + j]))
						printf("%c", data[i + j]);
					else
						printf(".");
				}
				else
					printf(" ");
			}
			printf("\n");
		}
	}
}

#define DEFINE_CONSOLEV2_PROPERTIES

// System headers
#include <windows.h>

// Standard library C-style
#include <wchar.h>
#include <stdlib.h>
#include <stdio.h>

#define ESC "\x1b"
#define CSI "\x1b["

extern "C"
{
	HANDLE hOut;
	HANDLE hIn;

	INPUT_RECORD records[128];

	bool GetStdIOHandles()
	{
		hIn = GetStdHandle(STD_INPUT_HANDLE);
		if (hIn == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		// Set output mode to handle virtual terminal sequences
		hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		return true;
	}

	bool EnableVTMode()
	{
		DWORD dwMode = 0;
		if (!GetConsoleMode(hOut, &dwMode))
		{
			return false;
		}

		dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		if (!SetConsoleMode(hOut, dwMode))
		{
			return false;
		}
		return true;
	}

#if 0
	void PrintVerticalBorder()
	{
		printf(ESC "(0"); // Enter Line drawing mode
		printf(CSI "104;93m"); // bright yellow on bright blue
		printf("x"); // in line drawing mode, \x78 -> \u2502 "Vertical Bar"
		printf(CSI "0m"); // restore color
		printf(ESC "(B"); // exit line drawing mode
	}

	void PrintHorizontalBorder(COORD const Size, bool fIsTop)
	{
		printf(ESC "(0"); // Enter Line drawing mode
		printf(CSI "104;93m"); // Make the border bright yellow on bright blue
		printf(fIsTop ? "l" : "m"); // print left corner 

		for (int i = 1; i < Size.X - 1; i++)
			printf("q"); // in line drawing mode, \x71 -> \u2500 "HORIZONTAL SCAN LINE-5"

		printf(fIsTop ? "k" : "j"); // print right corner
		printf(CSI "0m");
		printf(ESC "(B"); // exit line drawing mode
	}

	void PrintStatusLine(char* const pszMessage, COORD const Size)
	{
		printf(CSI "%d;1H", Size.Y);
		printf(CSI "K"); // clear the line
		printf(pszMessage);
	}
#endif

	int CON_Init(void)
	{
		// Get handles for Console I/O
		bool fSuccess = GetStdIOHandles();
		if (!fSuccess)
		{
			return -1;
		}

		// Enable Console VT mode
		fSuccess = EnableVTMode();
		if (!fSuccess)
		{
			//DWORD dw = GetLastError();
			return -1;
		}

		// Get Screen dimensions
		CONSOLE_SCREEN_BUFFER_INFO ScreenBufferInfo;
		GetConsoleScreenBufferInfo(hOut, &ScreenBufferInfo);
		COORD Size;
		Size.X = ScreenBufferInfo.srWindow.Right - ScreenBufferInfo.srWindow.Left + 1;
		Size.Y = ScreenBufferInfo.srWindow.Bottom - ScreenBufferInfo.srWindow.Top + 1;

#if 0
		int ch;

		// Enter the alternate buffer
		printf(CSI "?1049h");

		// Clear screen, tab stops, set, stop at columns 16, 32
		printf(CSI "1;1H");
		printf(CSI "2J"); // Clear screen

		int iNumTabStops = 4; // (0, 20, 40, width)
		printf(CSI "3g"); // clear all tab stops
		printf(CSI "1;20H"); // Move to column 20
		printf(ESC "H"); // set a tab stop

		printf(CSI "1;40H"); // Move to column 40
		printf(ESC "H"); // set a tab stop

						 // Set scrolling margins to 3, h-2
		printf(CSI "3;%dr", Size.Y - 2);
		int iNumLines = Size.Y - 4;

		printf(CSI "1;1H");
		printf(CSI "102;30m");
		printf("Windows 10 Anniversary Update - VT Example");
		printf(CSI "0m");

		// Print a top border - Yellow
		printf(CSI "2;1H");
		PrintHorizontalBorder(Size, true);

		// // Print a bottom border
		printf(CSI "%d;1H", Size.Y - 1);
		PrintHorizontalBorder(Size, false);

		// draw columns
		printf(CSI "3;1H");
		int line = 0;
		for (line = 0; line < iNumLines * iNumTabStops; line++)
		{
			PrintVerticalBorder();
			if (line + 1 != iNumLines * iNumTabStops) // don't advance to next line if this is the last line
				printf("\t"); // advance to next tab stop
		}

		PrintStatusLine("Press any key to see text printed between tab stops.", Size);
		ch = CON_getc();

		// Fill columns with output
		printf(CSI "3;1H");
		for (line = 0; line < iNumLines; line++)
		{
			int tab = 0;
			for (tab = 0; tab < iNumTabStops - 1; tab++)
			{
				PrintVerticalBorder();
				printf("line=%d", line);
				printf("\t"); // advance to next tab stop
			}
			PrintVerticalBorder();// print border at right side
			if (line + 1 != iNumLines)
				printf("\t"); // advance to next tab stop, (on the next line)
		}

		PrintStatusLine("Press any key to demonstrate scroll margins", Size);
		ch = CON_getc();

		printf(CSI "3;1H");
		for (line = 0; line < iNumLines * 2; line++)
		{
			printf(CSI "K"); // clear the line
			int tab = 0;
			for (tab = 0; tab < iNumTabStops - 1; tab++)
			{
				PrintVerticalBorder();
				printf("line=%d", line);
				printf("\t"); // advance to next tab stop
			}
			PrintVerticalBorder(); // print border at right side
			if (line + 1 != iNumLines * 2)
			{
				printf("\n"); //Advance to next line. If we're at the bottom of the margins, the text will scroll.
				printf("\r"); //return to first col in buffer
			}
		}
		PrintStatusLine("Press any key to exit", Size);
		ch = CON_getc(); #endif
#endif
			return 0;
	}

	int CON_kbhit()
	{
		DWORD NumberOfEventsRead;
		BOOL rc = PeekConsoleInput(hIn, records, 128, &NumberOfEventsRead);
		if (rc)
		{
			int count = 0;
			//printf("\nNumberOfEventsRead=%d\n", NumberOfEventsRead);
			for (unsigned int i = 0; i < NumberOfEventsRead; i++)
			{
				WORD type = records[i].EventType;
				if (type == KEY_EVENT)
				{
					if (records[i].Event.KeyEvent.bKeyDown)
					{
						//printf(" %02d: type=%04x Key pressed\n", i, type);
						count++;
					}
				}
			}
			return count;
		}
		return 0;
	}

	int CON_getc()
	{
		DWORD NumberOfEventsTotal;
		DWORD NumberOfEventsRead;
		while (1)
		{
			BOOL rc = GetNumberOfConsoleInputEvents(hIn, &NumberOfEventsTotal);
			if (rc)
			{
				if (NumberOfEventsTotal > 0)
				{
					//printf("\nNumberOfEventsTotal=%d\n", NumberOfEventsTotal);
					BOOL rc = ReadConsoleInput(hIn, records, 1, &NumberOfEventsRead);
					if (rc)
					{
						if (NumberOfEventsRead > 0)
						{
							WORD type = records[0].EventType;
							if (type == KEY_EVENT)
							{
								DWORD state = records[0].Event.KeyEvent.dwControlKeyState;
								WORD kc = records[0].Event.KeyEvent.wVirtualKeyCode;
								WORD sc = records[0].Event.KeyEvent.wVirtualScanCode;
								WORD ch = records[0].Event.KeyEvent.uChar.AsciiChar;
								if (records[0].Event.KeyEvent.bKeyDown)
								{
									//printf(" type=%04x state=%04x sc=%04x kc=%04x ch=%02x ", type, state, sc, kc, ch);
									if (state & ENHANCED_KEY)
									{
										//printf("Enhanced key\n");
										return 0x100 | kc;
									}
									else if (state & SHIFT_PRESSED)
									{
										// a CTRL key pressed
										if (ch != 0x00)
										{
											//printf("Shift\n");
											return ch;
										}
										//printf("Shift?\n");
									}
									else if (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
									{
										// a CTRL key pressed
										if (ch != 0x00)
										{
											//printf("Control\n");
											return ch;
										}
										//printf("Control?\n");
									}
									else if (state & 0x1FF)
									{
										// Ignore unsupported virtual scan key combinations.
										//printf("Unsupported\n");
									}
									else
									{
										//printf("AsciiChar\n");
										return ch;
									}
								}
							}
						}
					}
				}
			}
		}
		return 0;
	}

#include "bsp.h"
#include "cfg.h"
#include "third_party/term/term.h"
#include "evt.h"
#include "cli.h"
#include "pts.hpp"

	extern void TRM_Init();
	extern void TRM_Term();
	extern int Cmd_ted(int argc, char* argv[]);
	extern int Cmd_pic(int argc, char* argv[]);

	//*****************************************************************************
	// This function implements the "help" command.  It prints a list of available
	// commands with a brief description.
	//*****************************************************************************
	int Cmd_help(int argc, char* argv[])
	{
		SHELL_COMMAND* psEntry;

		// Point at the beginning of the command table.
		psEntry = &g_psShellCmds[0];

		// Enter a loop to read each entry from the command table.  The end of the
		// table has been reached when the command name is NULL.
		while (psEntry->cmd)
		{
			// Print the command name and the brief description.
			CON_printf("%6s: %s\n", psEntry->cmd, psEntry->help);

			// Advance to the next entry in the table.
			psEntry++;
		}
		return(0);
	}

	bool exit_flag = false;

	int Cmd_exit(int argc, char* argv[])
	{
		exit_flag = true;
		return(0);
	}

	//*****************************************************************************
	// This is the table that holds the command names, implementing functions, and
	// brief description.
	//*****************************************************************************
	SHELL_COMMAND g_psShellCmds[] =
	{
	{ "help",       Cmd_help,       "display list of commands" },
	{ "?",          Cmd_help,       "alias for help" },
	{ "cfg",        Cmd_cfg,        "configure settings" },
	#if HAS_PTS == 1
	{ "pts",        Cmd_pts,        "protothread scheduler" },
	#endif
	#if HAS_LOG == 1
	{ "log",        Cmd_log,        "logging options" },
	#endif    
	#if HAS_CLI == 1
	{ "cli",        Cmd_cli,        "command lines" },
	#endif
	#if HAS_EVT == 1
	{ "evt",        Cmd_evt,        "events" },
	#endif
	{ "ed",         Cmd_ted,        "text editor" },
	{ "pic",        Cmd_pic,        "pico C" },
	{ "exit",       Cmd_exit,       "exit" },
	{ 0, 0, 0 }
	};

}

class SysTick : CTimerFunc
{
public:
	SysTick() {}
	void Func(void) { SysTickIntHandler(); }
};

int main(int argc, char* argv[])
{
	argc; // unused
	argv; // unused

	// Capture output from putchar, puts and printf macros.
	LOG_Init((char *)"c:\\temp\\term.log");
	BSP_Init();
	TMR_Init(SYSTICK_RATE_HZ);	// 100Hz timebase

	SysTick* Tick = new SysTick();
	TMR_Event(1, (CTimerEvent*)Tick, PERIODIC);
	/*
		printf("\nset 1 second timer ");
		Tmr t = TMR_New();
		TMR_Start(t, 10);
		while (!TMR_IsTimeout(t))
		{
			TMR_Delay(1);
			putchar('.');
		}
		printf("done\n");

		uint8_t t1 = BSP_claim_msec_cnt();
		BSP_reset_msec_cnt(t1);
		while (1)
		{
			TMR_Delay(1);
			int count = BSP_get_msec_cnt(t1);
			if (count > 10000)
			{
				break;
			}
			printf("%5d\b\b\b\b\b", count);
		}
		printf("\n");
		BSP_return_msec_cnt(t1);
	*/
	if (CON_Init() != 0)
	{
		printf("Unable to enter VT processing mode. Quitting.\n");
		return -1;
	}

#if HAS_PTS == 1
	PTS_Init();
#endif
#if HAS_CLI == 1
	CLI_Init();
#endif

#if HAS_PTS == 1
	while (PTS_GetTaskCnt() > 0)
	{
		PTS_Process();

		if (exit_flag)
			break;
	}
#endif

#if 0
	// Exit the alternate buffer
	printf(CSI "?1049l");
#endif

	TRM_Term();
	//PTS_Term();
	TMR_Term();
	// Close capture log file.
	LOG_Term();

	return 0;
}


// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
