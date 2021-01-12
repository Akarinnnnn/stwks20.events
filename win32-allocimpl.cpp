#include "pch.h"
#include "events.hpp"

void steam::events::sthread_dispatcher::AllocBuff()
{
	parambuff = (unsigned char*)::VirtualAlloc(nullptr, buffsize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	if (!parambuff)
		throw std::bad_alloc();
}

void steam::events::sthread_dispatcher::FreeBuff()
{
	if (parambuff)
		::VirtualFree(parambuff, 0, MEM_RELEASE);
}