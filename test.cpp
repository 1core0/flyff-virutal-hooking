bool AlreadyHooked = false;
UINT32 gRWXBuf = NULL;
UINT32 gSendActMsgOrig = NULL;
UINT32 gStrongBowEnabled = NULL;
void BowHack()
{
	auto GameBase = GetBaseAddress();
	if (!GameBase)
	{
		return;
	}

	// SendActMsg: Neuz.exe + 19B780 | 55 8B EC F6 41 08 08 74 ??
	// Inject shellcode if not already done
	if (!AlreadyHooked)
	{
		// Get CActionMover Object
		UINT32 Ptr1 = *(UINT32*)(GameBase + 0x004FFA94);
		if (Ptr1)
		{
			UINT32 CActionMoverObj = *(UINT32*)(Ptr1 + 0x33C);
			if (CActionMoverObj)
			{
				DbgPrint("CActionMoverObj @ %X\n", CActionMoverObj);

				// First 16 Bytes used for Shadow VMT
				UINT32 CActionMoverObjVtable = *(UINT32*)CActionMoverObj;
				if (CActionMoverObjVtable)
				{
					DbgPrint("CActionMoverObjVtable: %X\n", CActionMoverObjVtable);

					// Allocate space for vmt & shellcode
					/*
					16 bytes - ShadowVMT
					4  bytes - Used for Settings
					?  bytes - Shellcode
					*/
					if (gRWXBuf == NULL)
					{
						SIZE_T RWXBufSize = PAGE_SIZE;
						fZwAllocateVirtualMemory(NtCurrentProcess(), (PVOID*)&gRWXBuf, 0, &RWXBufSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
						DbgPrint("gRWXBuf @ %X\n", gRWXBuf);
					}

					memcpy((void*)gRWXBuf, (void*)CActionMoverObjVtable, 16);

					// Save SendActMsg 
					gSendActMsgOrig = *(UINT32*)(CActionMoverObjVtable + 4);
					DbgPrint("gSendActMsgOrig @ %X\n", gSendActMsgOrig);

					// Save gStrongBowEnabled Ptr
					gStrongBowEnabled = (UINT32)(gRWXBuf + 16);

					// Setup shellcode
					unsigned char Shellcode[] =
					{
						0x50,				// push eax
						0x8B, 0x45, 0xDC,	// mov eax, [ebp-0x24] | eax now holds dwItemId
						0x83, 0xF8, 0x00,	// cmp eax, 0
						0x75, 0x11,			// jne $JMPBACK

						0xA1, 0xAA, 0xAA, 0xAA, 0xAA, // mov eax, [StrongBowEnabled]
						0x83, 0xF8, 0x01,   // cmp eax, 1

						0x75, 0x07,			// jne $JMPBACK
						0xC7, 0x45, 0xDC, 0x04, 0x00, 0x00, 0x00, // mov [ebp-0x24], 4

						// $JMPBACK
						0x58,  // pop eax
						0xE9, 0xBB, 0xBB, 0xBB, 0xBB, // jmp [gSendActMsgOrig]
					};

					*(UINT32*)(Shellcode + 10) = (UINT32)gStrongBowEnabled; // StrongBowEnabled
					*(UINT32*)(Shellcode + 28) = (UINT32)(gSendActMsgOrig - (gRWXBuf + 47) - 5); // jmp back

					// Copy Shellcode
					memcpy((void*)(gRWXBuf + 20), Shellcode, sizeof(Shellcode));

					// Patch VMT Ptr
					*(UINT32*)(CActionMoverObjVtable + 4) = gRWXBuf + 20;
					DbgPrint("VMT Hook placed :)\n");

					AlreadyHooked = true;
				}
			}
		}
		
	}

	if (AlreadyHooked)
	{
		if (gItems.bowAlwaysStrongAttack)
			*(UINT32*)gStrongBowEnabled = 1;
		else
			*(UINT32*)gStrongBowEnabled = 0;
	}

}
