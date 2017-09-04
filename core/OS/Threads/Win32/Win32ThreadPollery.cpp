#include "Win32ThreadPoller.h"
#include <stdio.h>
namespace OS {
	CWin32ThreadPoller::CWin32ThreadPoller() {
		m_poll_value = 0;
		m_wait_value = 1;
		m_bad_value = 0;
		m_signal_count = 0;
	}
	CWin32ThreadPoller::~CWin32ThreadPoller() {
	}
	bool CWin32ThreadPoller::wait() {
		if (m_signal_count == 0) {
			m_poll_value = m_wait_value;
			WaitOnAddress(&m_poll_value, &m_wait_value, sizeof(ULONG), INFINITE);
		}
		m_signal_count = 0;
		return true;
	}
	void CWin32ThreadPoller::signal() {
		m_signal_count++;
		if (m_poll_value != m_bad_value) {
			m_poll_value = m_bad_value;
			WakeByAddressSingle(&m_poll_value);
		}
	}
}