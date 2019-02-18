/*
使用C++11的lambda实现资源的释放，防止资源泄露
使用方式，在资源创建之后，添加以下语句
NS_SCOPE_GUARD::ON_SCOPE_EXIT([&] {释放资源});
*/
#pragma once

#include <functional>

namespace NS_SCOPE_GUARD {

	class ScopeGuard
	{
	public:
		typedef std::function<void()> Function;
	public:
		explicit ScopeGuard(Function onExitFunc)
			: _onExitFunc(onExitFunc), _dismissed(false)
		{

		}
		~ScopeGuard()
		{
			if (!_dismissed)
			{
				_onExitFunc();
			}
		}

		// 某种情况下放弃资源释放
		void dismiss(void)
		{
			_dismissed = true;
		}
	private:
		Function _onExitFunc;
		bool _dismissed;

	private:
		ScopeGuard(const ScopeGuard&);
		ScopeGuard& operator=(const ScopeGuard&);
	};

#define SCOPE_LOCAL_NAME_CAT(name,line) name##line
#define SCOPE_LOCAL_NAME(name,line) SCOPE_LOCAL_NAME_CAT(name,line)
#define ON_SCOPE_EXIT(func) ScopeGuard SCOPE_LOCAL_NAME(sg,__LINE__)(func)

};
