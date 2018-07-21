#ifdef _TEST_DRIVER_H
#define _TEST_DRIVER_H
namespace Test {
    class TestNetIOSocket;
    class TestDriver {
        public:
            TestDriver(INetDriver *driver);
            ~TestDriver();

            TestNetIOSocket *make_FakeConnection(OS::Address source_address);
        private:
            INetDriver *mp_test_driver;
    };
}

#endif //_TEST_DRIVER_H