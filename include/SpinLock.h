#pragma once
#include <atomic>

class SpinLock
{
public:
	void Lock()
	{
		while (flag.test_and_set())
		{
		}
	}

	void UnLock()
	{
		flag.clear();
	}

private:
	std::atomic_flag flag;
};
