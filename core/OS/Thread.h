#ifndef _OS_CTHREAD_H
#define _OS_CTHREAD_H
namespace OS {
    class CThread;
    typedef void *(ThreadEntry)(CThread *);
    class CThread {

    public:
        CThread(ThreadEntry *entry, void *params, bool auto_start = true) { m_entry = entry; m_start_on_create = auto_start; m_params = params; m_thread_dead = false;};
        virtual ~CThread() { };
        virtual void start() = 0;
        virtual void stop() = 0;
        void *getParams() {return m_params; };
    protected:
        ThreadEntry *m_entry;
        bool m_start_on_create;
        bool m_running;
        void *m_params;

        bool m_thread_dead;
    };
}
#endif //_CORE_CTHREAD_H