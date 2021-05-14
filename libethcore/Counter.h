#pragma once
namespace dev {
    namespace eth {

        template<typename T>
        class Counter {
        public:
            Counter() { ++count; }

            Counter(const Counter &) { ++count; }

            ~Counter() { --count; }

            static uint64_t howMany() { return count; }

        private:
            static std::atomic <uint64_t> count;
        };

        template<typename T> std::atomic <uint64_t> Counter<T>::count = 0;

    }
}