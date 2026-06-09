#pragma once

#include <cstdint>

#define WEAK __declspec(selectany)
#define NAKED __declspec(naked)

// MP/SP selection removed (environment gone): every symbol is name-keyed (AddrMap),
// and all CALL_ADDR sites pass mp=0x0 (MP was never wired). SELECT now yields sp.
#define SELECT(mp, sp) (sp)
#define ASSIGN(type, mp, sp) reinterpret_cast<type>(SELECT(mp, sp))
#define CALL_ADDR(mp, sp) ASSIGN(void*, mp, sp)

// Resolved by AddrMap.cpp; variant-aware address lookup used by symbol<> (name-keyed form).
namespace T4M 
{ 
	std::uintptr_t GetAddress(const char*); 
}

namespace T4
{
	namespace engine
	{
		// Name-keyed symbol: resolved lazily via T4M::GetAddress (variant-aware, see
		// AddrMap.cpp). The name is the key in addr_mapping.csv.
		template <typename T>
		class symbol
		{
		public:
			explicit symbol(const char* name) : name_(name), cached_(nullptr)
			{
			}

			T* get() const
			{
				if (!cached_)
					cached_ = reinterpret_cast<T*>(T4M::GetAddress(name_)); // lazy + cache (runtime only)
				return cached_;
			}

			void set(const size_t ptr)
			{
				this->cached_ = reinterpret_cast<T*>(ptr);
			}

			operator T* () const
			{
				return this->get();
			}

			T* operator->() const
			{
				return this->get();
			}

			// &gX -> base (call sites verbatim)
			T* operator& () const
			{
				return this->get();
			}

			// gX = p (re-point relocated pool)
			symbol& operator=(T* p)
			{
				this->cached_ = p;
				return *this;
			}


		private:
			const char* name_;
			mutable T* cached_;
		};


		//extern AimAssistGlobals* aaGlobArray;
		//extern WeaponDef** bg_weaponDefs;

		//float DiffTrackAngle(int a1, int a2, float a3, float a4);
		//float AngleSubtract(double a1, double a2);
	}
} // namespace T4M
