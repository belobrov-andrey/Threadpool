// workaround
#define _GLIBCXX_USE_NANOSLEEP

#include <iostream>
#include <vector>
#include <chrono>

#include "Threadpool.hpp"

int main() {
    ThreadPool pool(4);

    typedef std::vector<std::future<int>> rvec_t;
    rvec_t results;

    for(auto i = 0; i < 8; ++i) {
        results.push_back(
            pool.enqueue<int>([i] {
                std::cout << "hello " << i << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::cout << "world " << i << std::endl;

                return i * i;
            })
        );
    }

    for(rvec_t::size_type i = 0; i < results.size(); ++i)
        std::cout << results[i].get() << ' ';
    std::cout << std::endl;
}
