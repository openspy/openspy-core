#ifndef _OS_LINKED_LIST_H
#define _OS_LINKED_LIST_H

namespace OS {
    template<typename T>
    class LinkedListHead {
    public:
        LinkedListHead() {
            mp_head = NULL;
        }
        T GetHead() { return mp_head; };
        void SetHead(T item) { mp_head = item; }
        void RemoveFromList(T item) {
            if (item == GetHead()) {
                SetHead(item->GetNext());
            }
            else if (item->GetNext() != NULL) {
                item->GetNext()->SetNext(item->GetNext()->GetNext());

                T previous;
                if(FindPrevious(item, previous) )
                {
                    previous->SetNext(item->GetNext());
                }
            }
        }
        void AddToList(T item) {
            if (GetHead() == NULL) {
                SetHead(item);
            }
            else {
                T p = GetHead();
                while (p->GetNext() != NULL) {
                    p = p->GetNext();
                }
                p->SetNext(item);
            }
        }
    private:
        bool FindPrevious(T item, T &out) {
            T p = GetHead();
            if(p) {
                do {
                    T next = p->GetNext();
                    if(next == p) { 
                        out = p;
                        return true;
                    }
                    p = next;
                } while(p);
            }
            return false;
        }
        T mp_head;
    };

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


    
    template<typename T, typename S>
    class LinkedListIterator {
        typedef bool (*LinkedListIteratorCallback)(T item, S state);
        public:
            LinkedListIterator(LinkedListHead<T> *head) {
                mp_head = head;
            }
            void Iterate(LinkedListIteratorCallback callback, S state) {
                T p = mp_head->GetHead();
                if(p)   {
                    do {
                        T next = p->GetNext();
                        if(!callback(p, state)) {
                            break;
                        }
                        p = next;
                    } while(p);
                }
            }
        private:
            LinkedListHead<T> *mp_head;
    };
}
#endif //_OS_LINKED_LIST_H