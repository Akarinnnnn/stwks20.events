#include "events.hpp"
#include <thread>

namespace steam::events::dll
{
	using HSteamPipe = int32;
	using HSteamUser = int32;
	extern "C" __declspec(dllimport) HSteamPipe __cdecl SteamAPI_GetHSteamPipe();
	extern "C" __declspec(dllimport) HSteamUser __cdecl SteamAPI_GetHSteamUser();
	extern "C" __declspec(dllimport) HSteamPipe __cdecl SteamGameServer_GetHSteamPipe();
	extern "C" __declspec(dllimport) HSteamUser __cdecl SteamGameServer_GetHSteamUser();

	struct CallbackMsg_t
	{
		int32 m_hSteamUser; // Specific user to whom this callback applies.
		int m_iCallback; // Callback identifier.  (Corresponds to the k_iCallback enum in the callback structure.)
		uint8* m_pubParam; // Points to the callback structure
		int m_cubParam; // Size of the data pointed to by m_pubParam
	};

	struct SteamAPICallCompleted_t
	{
		constexpr static int callback_typeid = 700 + 3;
		SteamAPICall_t m_hAsyncCall;
		int m_iCallback;
		uint32 m_cubParam;
	};

	/// Inform the API that you wish to use manual event dispatch.  This must be called after SteamAPI_Init, but before
	/// you use any of the other manual dispatch functions below.
	extern "C" __declspec(dllimport) void __cdecl SteamAPI_ManualDispatch_Init();

	/// Perform certain periodic actions that need to be performed.
	extern "C" __declspec(dllimport) void __cdecl SteamAPI_ManualDispatch_RunFrame(HSteamPipe hSteamPipe);

	/// Fetch the next pending callback on the given pipe, if any.  If a callback is available, true is returned
	/// and the structure is populated.  In this case, you MUST call SteamAPI_ManualDispatch_FreeLastCallback
	/// (after dispatching the callback) before calling SteamAPI_ManualDispatch_GetNextCallback again.
	extern "C" __declspec(dllimport) bool __cdecl SteamAPI_ManualDispatch_GetNextCallback(HSteamPipe hSteamPipe, CallbackMsg_t * pCallbackMsg);

	/// You must call this after dispatching the callback, if SteamAPI_ManualDispatch_GetNextCallback returns true.
	extern "C" __declspec(dllimport) void __cdecl SteamAPI_ManualDispatch_FreeLastCallback(HSteamPipe hSteamPipe);

	/// Return the call result for the specified call on the specified pipe.  You really should
	/// only call this in a handler for SteamAPICallCompleted_t callback.
	extern "C" __declspec(dllimport) bool __cdecl SteamAPI_ManualDispatch_GetAPICallResult(HSteamPipe hSteamPipe, SteamAPICall_t hSteamAPICall, void* pCallback, int cubCallback, int iCallbackExpected, bool& pbFailed);
}

steam::events::sthread_dispatcher::sthread_dispatcher()
{
	dll::SteamAPI_ManualDispatch_Init();
	AllocBuff();
}

steam::events::sthread_dispatcher::~sthread_dispatcher()
{
	Shutdown();
	FreeBuff();
}

void steam::events::sthread_dispatcher::Shutdown() noexcept
{
	working = false;
}

void steam::events::sthread_dispatcher::RegisterCallresult(HandlerRecord& handler)
{
	crhandlers.emplace_front(&handler);
}

void steam::events::sthread_dispatcher::RegisterCallback(HandlerRecord& handler)
{
	cbhandlers.push_back(&handler);
}

void steam::events::sthread_dispatcher::UnRegisterCallResult(HandlerRecord* handler)
{
	auto next = ++crhandlers.begin();
	for (auto iter = crhandlers.cbegin(); next != crhandlers.cend(); ++iter, ++next)
		if (*next == handler)
			crhandlers.erase_after(iter);
}

void steam::events::sthread_dispatcher::UnRegisterCallback(HandlerRecord* handler)
{
	for (auto iter = cbhandlers.cbegin(); iter != cbhandlers.cend(); ++iter)
		if (*iter == handler)
			cbhandlers.erase(iter);
}

steam::events::sthread_dispatcher::EHFunction& steam::events::sthread_dispatcher::EH() { return eh; }

bool steam::events::sthread_dispatcher::IsEHInsatlled() { return (bool)eh; }

void steam::events::sthread_dispatcher::operator()(void) noexcept
{
	dll::CallbackMsg_t msg;
	auto pipe = dll::SteamAPI_GetHSteamPipe();
	auto* buffer = this->parambuff;
	dll::SteamAPICallCompleted_t* apicall = nullptr;
	bool async_iofail = false;
	while (working)
	{
		dll::SteamAPI_ManualDispatch_RunFrame(pipe);

		while (dll::SteamAPI_ManualDispatch_GetNextCallback(pipe, &msg))
		{
			if (msg.m_iCallback == dll::SteamAPICallCompleted_t::callback_typeid) [[likely]] // callresult
			{
				apicall = reinterpret_cast<dll::SteamAPICallCompleted_t*>(msg.m_pubParam);
				dll::SteamAPI_ManualDispatch_GetAPICallResult(
					pipe,
					apicall->m_hAsyncCall,
					this->parambuff,
					apicall->m_cubParam,
					apicall->m_iCallback,
					async_iofail
				);

				auto next = ++crhandlers.begin();
				for (auto iter = crhandlers.cbegin(); next != crhandlers.cend(); ++iter, ++next)
				{
					auto* ptr = *iter;
					if (ptr->handle == apicall->m_hAsyncCall &&
						ptr->callback_typeid == apicall->m_iCallback)
					{
						try
						{
							ptr->Invoke(buffer, async_iofail);
						}
						catch (const std::exception& e)
						{
							if (eh) eh(e);
						}
					}
					crhandlers.erase_after(iter);
				}
			}
			else // callback
			{
				for (auto iter = cbhandlers.begin(); iter != cbhandlers.end(); ++iter)
				{
					auto ptr = *iter;
					if (ptr->callback_typeid == msg.m_iCallback)
					{
						try
						{
							ptr->Invoke(msg.m_pubParam, false);
						}
						catch (const std::exception& e)
						{
							if (eh) eh(e);
						}
					}
				}
			}

			dll::SteamAPI_ManualDispatch_FreeLastCallback(pipe);
		}
	}
}

steam::events::mthread_dispatcher* steam::events::mthread_dispatcher::instance = nullptr;

steam::events::mthread_dispatcher::~mthread_dispatcher()
{
	Shutdown();
}

steam::events::mthread_dispatcher::mthread_dispatcher()
{
	working = false;
}

void steam::events::mthread_dispatcher::Shutdown() noexcept
{
	std::lock_guard<std::mutex> a{ crlock }, b{ cblock };
	sthread_dispatcher::Shutdown();
}

template<bool readsafe>
void steam::events::mthread_dispatcher::thread_func(void) noexcept
{
	// 复制的
	dll::CallbackMsg_t msg;
	auto pipe = dll::SteamAPI_GetHSteamPipe();
	auto* buffer = this->parambuff;
	dll::SteamAPICallCompleted_t* apicall = nullptr;
	bool async_iofail = false;
	while (working)
	{
		dll::SteamAPI_ManualDispatch_RunFrame(pipe);
		while (dll::SteamAPI_ManualDispatch_GetNextCallback(pipe, &msg))
		{
			if (msg.m_iCallback == dll::SteamAPICallCompleted_t::callback_typeid) [[likely]] // callresult
			{
				apicall = reinterpret_cast<dll::SteamAPICallCompleted_t*>(msg.m_pubParam);
				dll::SteamAPI_ManualDispatch_GetAPICallResult(
					pipe,
					apicall->m_hAsyncCall,
					this->parambuff,
					apicall->m_cubParam,
					apicall->m_iCallback,
					async_iofail
				);
				if constexpr(readsafe)
				{
					std::lock_guard g{ crlock };
					auto next = ++crhandlers.begin();// noex
					for (auto iter = crhandlers.cbegin(); next != crhandlers.cend(); ++iter, ++next) //noex
					{
						auto* ptr = *iter;// noex
						if (ptr->handle == apicall->m_hAsyncCall &&
							ptr->callback_typeid == apicall->m_iCallback)
						{
							try //noex
							{
								ptr->Invoke(buffer, async_iofail);
							}
							catch (const std::exception& e)
							{
								if (eh) eh(e);
							}
						}

						crhandlers.erase_after(iter);
					}
				}
				else
				{
					auto next = ++crhandlers.begin();// noex
					for (auto iter = crhandlers.cbegin(); next != crhandlers.cend(); ++iter, ++next) //noex
					{
						auto* ptr = *iter;// noex
						if (ptr->handle == apicall->m_hAsyncCall &&
							ptr->callback_typeid == apicall->m_iCallback)
						{
							try //noex
							{
								ptr->Invoke(buffer, async_iofail);
							}
							catch (const std::exception& e)
							{
								if (eh) eh(e);
							}
						}
						std::lock_guard g{ crlock };
						crhandlers.erase_after(iter);
					}

				}
			}
			else // callback
			{
				for (auto iter = cbhandlers.begin(); iter != cbhandlers.end(); ++iter)
				{
					auto ptr = *iter;
					if (ptr->callback_typeid == msg.m_iCallback)
					{
						try
						{
							ptr->Invoke(msg.m_pubParam, false);
						}
						catch (const std::exception& e)
						{
							if (eh) eh(e);
						}
					}
				}
			}

			dll::SteamAPI_ManualDispatch_FreeLastCallback(pipe);
		}
		std::this_thread::yield();
	}
}

void steam::events::mthread_dispatcher::operator()(void) noexcept { thread_func<false>(); }
void steam::events::mthread_dispatcher::ReadSafeThreadFunction(void) noexcept { thread_func<true>(); }

void steam::events::mthread_dispatcher::RegisterCallresult(HandlerRecord& handler)
{
	std::lock_guard guard{ crlock };
	sthread_dispatcher::RegisterCallresult(handler);
}

void steam::events::mthread_dispatcher::RegisterCallback(HandlerRecord& handler)
{
	std::lock_guard guard{ cblock };
	sthread_dispatcher::RegisterCallback(handler);
}

void steam::events::mthread_dispatcher::UnRegisterCallResult(HandlerRecord* handler)
{
	std::lock_guard g{ crlock };
	sthread_dispatcher::UnRegisterCallResult(handler);
}

void steam::events::mthread_dispatcher::UnRegisterCallback(HandlerRecord* handler)
{
	std::lock_guard g{ cblock };
	sthread_dispatcher::UnRegisterCallback(handler);
}

void steam::events::mthread_dispatcher::Initialize()
{
	if (!instance)
		instance = new mthread_dispatcher();
}

void steam::events::mthread_dispatcher::StartThread(bool readSafe)
{
	Initialize();
	if (!instance->working)
	{
		instance->working = true;
		if (readSafe)
			std::thread{ &mthread_dispatcher::ReadSafeThreadFunction, instance }.detach();
		else
			std::thread{ &mthread_dispatcher::operator(), instance }.detach();
	}
}
steam::events::mthread_dispatcher& steam::events::mthread_dispatcher::Get()
{
	return *instance;
}

void steam::events::mthread_dispatcher::Destory()
{
	delete instance;
	instance = nullptr;
}

steam::events::DispatcherGuard::DispatcherGuard(bool readsafe)
{
	p = &mthread_dispatcher::Get();

	mthread_dispatcher::StartThread(readsafe);
}

steam::events::DispatcherGuard::~DispatcherGuard()
{
	mthread_dispatcher::Destory();
	p = nullptr;
}
