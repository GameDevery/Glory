#pragma once
#include "RenderQueue.h"
#include "Thread.h"

namespace Glory
{
	class GraphicsThread
	{
	public:
		GraphicsThread();
		virtual ~GraphicsThread();

		void Start();

	private:
		void Run();

	private:
		Thread* m_pThread;
		RenderQueue* m_pRenderQueue;
	};
}