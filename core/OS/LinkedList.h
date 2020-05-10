#ifndef _OS_LINKED_LIST_H
#define _OS_LINKED_LIST_H

namespace OS {
    template<typename T>
    class LinkedList {
        public:
            LinkedList() {
                mp_next = NULL;
            }        
            T GetNext() { return mp_next; }
            void SetNext(T item) { mp_next = item; }
        private:
            T mp_next;
    };
}
#endif //_OS_LINKED_LIST_H