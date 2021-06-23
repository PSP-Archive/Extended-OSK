/*
	Extended OSK
	Copyright (C) 2011, Total_Noob

	main.c: Main code

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <systemctrl.h>

#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("ExtendedOSK", 0x0006, 1, 0);

#define JAL_OPCODE	0x0C000000
#define J_OPCODE	0x08000000
#define SC_OPCODE	0x0000000C
#define JR_RA		0x03e00008

#define NOP	0x00000000

#define MAKE_CALL(a, f) _sw(JAL_OPCODE | (((u32)(f) & 0x3FFFFFFF) >> 2), a); 
#define MAKE_JUMP(a, f) _sw(J_OPCODE | (((u32)(f) & 0x3FFFFFFF) >> 2), a); 
#define MAKE_SYSCALL(a, n) _sw(SC_OPCODE | (n << 6), a);
#define JUMP_TARGET(x) ((x & 0x3FFFFFFF) << 2)
#define REDIRECT_FUNCTION(a, f) _sw(J_OPCODE | (((u32)(f) >> 2)  & 0x03ffffff), a);  _sw(NOP, a+4);
#define MAKE_DUMMY_FUNCTION0(a) _sw(0x03e00008, a); _sw(0x00001021, a+4);
#define MAKE_DUMMY_FUNCTION1(a) _sw(0x03e00008, a); _sw(0x24020001, a+4);

STMOD_HANDLER previous;

SceUID block_id;
wchar_t *copied_string = NULL;

int (* ReleaseText)();
int (* WriteLetter)(int sel);

int len_array;
int param_array;

int osk_param;

int WriteLetterPatched(int sel)
{
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	SceCtrlData pad;
	sceCtrlPeekBufferPositive(&pad, 1);

	if(pad.Lx == 0)
	{
		int len = _lw(_lw(osk_param) + len_array);
		u32 *array = (u32 *)(_lw(_lw(_lw(_lw(osk_param) + param_array) + 0x1E8) + 0x1C) + 0x34);
		wchar_t *string = (wchar_t *)array[0];

		if(copied_string) sceKernelFreePartitionMemory(block_id); 

		block_id = sceKernelAllocPartitionMemory(2, "", PSP_SMEM_Low, len * 2 + 1, NULL);
		if(block_id)
		{
			copied_string = sceKernelGetBlockHeadAddr(block_id);
			memset(copied_string, 0, len * 2 + 1);
			memcpy(copied_string, string, len * 2);
		}

		return 0;
	}
	else if(pad.Lx == 255 && copied_string)
	{
		_sw((u32)copied_string, osk_param - 0x30);
		sceKernelDcacheWritebackAll();
		ReleaseText();

		return 0;
	}

	return WriteLetter(sel);
}

int OnModuleStart(SceModule2 *mod)
{
	char *modname = mod->modname;
	u32 text_addr = mod->text_addr;

	if(strcmp(modname, "sceVshOSK_Module") == 0)
	{
		int inst = 0;

		int i;
		for(i = 0; i < mod->text_size; i += 4)
		{
			u32 addr = text_addr + i;

			if(_lw(addr) == 0x1440FFF8 && _lw(addr + 4) == 0x24060001)
			{
				len_array = (int)_lh(addr + 8);
				param_array = (int)_lh(addr + 0xC);
			}
			else if(_lw(addr) == 0x27BDFFB0 && _lw(addr + 8) == 0xAFB00040 && _lw(addr + 0x10) == 0x8E030008)
			{
				ReleaseText = (void *)addr;

				u32 high = (((int)_lh(addr + 0x4C)) << 16);
				u32 low = ((int)_lh(addr + 0x54));

				if(low >= 0x8000) high -= 0x10000;

				osk_param = high | low;
			}
			else if(_lw(addr) == 0x2C820015)
			{
				WriteLetter = (void *)addr - 4;
				MAKE_CALL((u32)&inst, addr - 4);
			}
		}

		for(i = 0; i < mod->text_size; i += 4)
		{
			u32 addr = text_addr + i;

			if(_lw(addr) == inst)
			{
				MAKE_CALL(addr, WriteLetterPatched);
			}
		}

		sceKernelDcacheWritebackAll();
	}

	if(!previous)
		return 0;

	return previous(mod);
}

int module_start(SceSize args, void *argp)
{
	previous = sctrlHENSetStartModuleHandler(OnModuleStart);
	return 0;
}