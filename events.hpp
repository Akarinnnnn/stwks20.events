#pragma once

#ifdef _MSC_VER
#ifdef STWKS17EVENTDISPATCHER_EXPORTS
#define DISPATCHER_API __declspec(dllexport)
#else
#define DISPATCHER_API __declspec(dllimport)
#endif
#else
#define DISPATCHER_API
#endif


#include "types.hpp"
#include <functional>
#include <concepts>
#include <mutex>
#include <forward_list>
namespace steam::events
{
	template <typename T>
	concept classic_param = requires(T param)
	{
		{T::k_iCallback} -> std::convertible_to<int>;
	};

	/// <summary>
	/// 具有HandleResult函数，可移动构造
	/// </summary>
	template<typename T, typename ParamT>
	concept result_handler = requires(T & handler, bool ioFail, const ParamT * param)
	{
		{handler.HandleResult(ioFail, param)} -> std::same_as<void>;

		requires std::move_constructible<T>;
	} &&
	classic_param<ParamT>;

	template<typename T, typename ParamT>
	concept callback_handler = requires (T & handler, ParamT * param)
	{
		{handler.HandleCallback(param) }->std::same_as<void>;
	} &&
	classic_param<ParamT>;

	class HandlerRecord
	{
	protected:
		HandlerRecord(int callback_typeid, SteamAPICall_t h) :callback_typeid(callback_typeid), handle(h) {}
	public:
		HandlerRecord(const HandlerRecord&) = delete;

		const int callback_typeid;
		const SteamAPICall_t handle;

		virtual void Invoke(const void* param, bool iofail) = 0;
		DISPATCHER_API virtual ~HandlerRecord() = default;
	};

	/// <summary>
	/// 
	/// </summary>
	template <classic_param T>
	class LambdaHandler final : public HandlerRecord
	{
	private:
		std::function<void(const T*, bool)> handler;
	public:
		using handler_t = std::function<void(const T*, bool)>;
		LambdaHandler(SteamAPICall_t handle, handler_t&& handler) : HandlerRecord(T::k_iCallback, handle), handler(std::move(handler)) {}
#pragma region 弃置的构造函数
		LambdaHandler() = delete;
		LambdaHandler(const LambdaHandler&) = delete;
		LambdaHandler(LambdaHandler&&) = delete;
#pragma endregion

		virtual void Invoke(const void* param, bool iofail) override
		{
			handler(reinterpret_cast<const T*>(param), iofail);
		}
		virtual ~LambdaHandler() override = default;
	};

	/// <summary>
	/// 适用于使用steamworks api的小工具，如下载mod
	/// </summary>
	class sthread_dispatcher
	{
	protected:
		std::forward_list<HandlerRecord*> crhandlers;
		std::vector<HandlerRecord*> cbhandlers;
		unsigned char* parambuff = nullptr;
		uint32_t buffsize = 4096u * 4u;// 4*4K

		bool working = true;

		// 所有平台都必须实现
		// void AllocBuff()，该函数分配4*4K的连续内存，用于接收steam事件参数
		// void FreeBuff()，该函数释放AllocBuff分配的缓冲区
		// 建议一次性分配4个连续的4K分页
		void AllocBuff();
		void FreeBuff();

		std::function<void(const std::exception&)> eh;
	public:
		using EHFunction = std::function<void(const std::exception&)>;

		void operator()(void) noexcept;

		DISPATCHER_API sthread_dispatcher();
		DISPATCHER_API ~sthread_dispatcher();

		DISPATCHER_API void Shutdown() noexcept;
		DISPATCHER_API EHFunction& EH();
		DISPATCHER_API bool IsEHInsatlled();

		DISPATCHER_API void RegisterCallresult(HandlerRecord& handler);
		DISPATCHER_API void RegisterCallback(HandlerRecord& handler);

		DISPATCHER_API void UnRegisterCallResult(HandlerRecord* handler);
		DISPATCHER_API void UnRegisterCallback(HandlerRecord* handler);
	};

	/// <summary>
	/// 
	/// </summary>
	class mthread_dispatcher final : private sthread_dispatcher
	{
	private:
		std::mutex crlock;
		std::mutex cblock;

		template<bool readsafe>
		void thread_func(void) noexcept;
		mthread_dispatcher();

		static mthread_dispatcher* instance;

	public:
		DISPATCHER_API ~mthread_dispatcher();

		using sthread_dispatcher::EHFunction;
		using sthread_dispatcher::EH;
		using sthread_dispatcher::IsEHInsatlled;

		DISPATCHER_API void Shutdown() noexcept;

		DISPATCHER_API void operator() (void) noexcept;
		DISPATCHER_API void ReadSafeThreadFunction(void) noexcept;

		DISPATCHER_API void RegisterCallback(HandlerRecord& handler);
		DISPATCHER_API void RegisterCallresult(HandlerRecord& handler);

		DISPATCHER_API void UnRegisterCallResult(HandlerRecord* handler);
		DISPATCHER_API void UnRegisterCallback(HandlerRecord* handler);
		
		DISPATCHER_API static void Initialize();
		DISPATCHER_API static void StartThread(bool isReadSafe = false);
		DISPATCHER_API static void Destory();

		/// <summary>
		/// 获取单例，可以缓存返回值
		/// </summary>
		DISPATCHER_API static mthread_dispatcher& Get();
	};
}

namespace steam::events
{
	/// <summary>
	/// <para>DispatcherGuard g;</para>
	/// <para>mthread_dispatcher* dispatcher = g.Get();</para>
	/// </summary>
	struct [[deprecated("use mthread_dispatcher::Get() instead")]] DispatcherGuard
	{
	private:
		mthread_dispatcher* p;
	public:
		DISPATCHER_API ~DispatcherGuard();
		DISPATCHER_API DispatcherGuard(bool isReadSafe = false);

		mthread_dispatcher* Get() { return p; }
	};
}