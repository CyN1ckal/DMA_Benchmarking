#include <Windows.h>
#include <string>
#include <iostream>
#include <memory>
#include "vmmdll.h"
#include "tracy/Tracy.hpp"

namespace Offsets {
	static uintptr_t GWorld = 0x9FE9998;	    // ?GWorld@@3VUWorldProxy@@A					48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 89 ? ? ? ? 49 8B D6
	static uintptr_t GNamePool = 0x9FEBDC0;		// NamePoolData							        FF C0 C1 EA ? 3B D0
	static uintptr_t GObjects = 0x9D0FF10;	    // ?GUObjectArray@@3VFUObjectArray@@A			0F 4C 15 ? ? ? ? 89 15 ? ? ? ? 48 81 C4
}

class DMA
{
public:
	VMM_HANDLE vmh = 0;
	DWORD PID = 0;
	uintptr_t BaseAddress = 0;
	bool Initialize(std::string ProcessName);
	bool Close();
};

bool gAlive = true;

bool DMA::Initialize(std::string ProcessName)
{
	LPCSTR args[] = { "","-device","FPGA://algo=0" };

	vmh = VMMDLL_Initialize(3, args);

	if (!vmh)
	{
		printf("VMMDLL_Initialize Failed!\n");
		return 0;
	}

	VMMDLL_PidGetFromName(vmh, ProcessName.c_str(), &PID);

	if (!PID)
	{
		printf("VMMDLL_PidGetFromName Failed!\n");
		VMMDLL_Close(vmh);
		return 0;
	}

	BaseAddress = VMMDLL_ProcessGetModuleBaseU(vmh, PID, ProcessName.c_str());

	if (!BaseAddress)
	{
		printf("VMMDLL_ProcessGetModuleBaseU Failed!\n");
		VMMDLL_Close(vmh);
		return 0;
	}

	return 1;
}

bool DMA::Close()
{
	if (!vmh)
		return 1;

	VMMDLL_Close(vmh);

	vmh = 0;

	return 1;
}

int main()
{
	DMA dma;

	if (!dma.Initialize("ArkAscended.exe"))
		return 0;

	while (gAlive)
	{
		ZoneScopedN("Main Loop");

		if (GetAsyncKeyState(VK_END))
			gAlive = 0;

		uintptr_t GWorldPtrAddress = dma.BaseAddress + Offsets::GWorld;

		uintptr_t GWorldAddress = 0x0;

		auto vmsh = VMMDLL_Scatter_Initialize(dma.vmh, dma.PID, VMMDLL_FLAG_NOCACHE);

		VMMDLL_Scatter_PrepareEx(vmsh, GWorldPtrAddress, 0x8, (BYTE*)&GWorldAddress, nullptr);

		VMMDLL_Scatter_Execute(vmsh);

		VMMDLL_Scatter_Clear(vmsh, dma.PID, VMMDLL_FLAG_NOCACHE);

		uintptr_t PersistentLevelPtrAddress = GWorldAddress + 0x2F0;

		uintptr_t PersistentLevelAddress = 0x0;

		VMMDLL_Scatter_PrepareEx(vmsh, PersistentLevelPtrAddress, 0x8, (BYTE*)&PersistentLevelAddress, nullptr);

		VMMDLL_Scatter_Execute(vmsh);

		VMMDLL_Scatter_Clear(vmsh, dma.PID, VMMDLL_FLAG_NOCACHE);

		uintptr_t ActorsAddress = PersistentLevelAddress + 0x98;

		struct TArray
		{
			uintptr_t* Data;
			int32_t NumElements;
			int32_t MaxElements;
		};

		TArray Actors = { 0,0,0 };

		VMMDLL_Scatter_PrepareEx(vmsh, ActorsAddress, sizeof(TArray), (BYTE*)&Actors, nullptr);

		VMMDLL_Scatter_Execute(vmsh);

		VMMDLL_Scatter_Clear(vmsh, dma.PID, VMMDLL_FLAG_NOCACHE);

		auto ActorPtrs = std::make_unique<uintptr_t[]>(Actors.NumElements);

		VMMDLL_Scatter_PrepareEx(vmsh, (uintptr_t)Actors.Data, sizeof(uintptr_t) * Actors.NumElements, (BYTE*)ActorPtrs.get(), nullptr);

		VMMDLL_Scatter_Execute(vmsh);

		VMMDLL_Scatter_Clear(vmsh, dma.PID, VMMDLL_FLAG_NOCACHE);

		auto ActorCheckBytes = std::make_unique<BYTE[]>(Actors.NumElements);

		for (int i = 0; i < Actors.NumElements; i++)
		{
			if (!ActorPtrs[i])
				continue;

			uintptr_t ByteCheckAddress = ActorPtrs[i] + 0x30;

			VMMDLL_Scatter_PrepareEx(vmsh, ByteCheckAddress, sizeof(BYTE), (BYTE*)&ActorCheckBytes[i], nullptr);
		}

		VMMDLL_Scatter_Execute(vmsh);

		VMMDLL_Scatter_CloseHandle(vmsh);
	}

	dma.Close();

	return 1;
}