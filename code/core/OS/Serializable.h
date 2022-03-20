#ifndef _OS_SERIALIZABLE_H
#define _OS_SERIALIZABLE_H
namespace OS {
    template<typename I, typename O> 
    class Serializable {
        virtual O Serialize(I input) = 0;
        virtual I Deserialize(O input) = 0;
    };
}
#endif //_OS_SERIALIZABLE_H